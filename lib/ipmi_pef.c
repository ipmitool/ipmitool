/*
 * Copyright (c) 2004 Dell Computers.  All Rights Reserved.
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
 * Neither the name of Dell Computers, or the names of
 * contributors may be used to endorse or promote products derived
 * from this software without specific prior written permission.
 * 
 * This software is provided "AS IS," without a warranty of any kind.
 * ALL EXPRESS OR IMPLIED CONDITIONS, REPRESENTATIONS AND WARRANTIES,
 * INCLUDING ANY IMPLIED WARRANTY OF MERCHANTABILITY, FITNESS FOR A
 * PARTICULAR PURPOSE OR NON-INFRINGEMENT, ARE HEREBY EXCLUDED.
 * DELL COMPUTERS ("DELL") AND ITS LICENSORS SHALL NOT BE LIABLE
 * FOR ANY DAMAGES SUFFERED BY LICENSEE AS A RESULT OF USING, MODIFYING
 * OR DISTRIBUTING THIS SOFTWARE OR ITS DERIVATIVES.  IN NO EVENT WILL
 * DELL OR ITS LICENSORS BE LIABLE FOR ANY LOST REVENUE, PROFIT OR DATA,
 * OR FOR DIRECT, INDIRECT, SPECIAL, CONSEQUENTIAL, INCIDENTAL OR
 * PUNITIVE DAMAGES, HOWEVER CAUSED AND REGARDLESS OF THE THEORY OF
 * LIABILITY, ARISING OUT OF THE USE OF OR INABILITY TO USE THIS SOFTWARE,
 * EVEN IF DELL HAS BEEN ADVISED OF THE POSSIBILITY OF SUCH DAMAGES.
 */

#include <string.h>
#include <math.h>
#include <time.h>

#include <ipmitool/bswap.h>
#include <ipmitool/helper.h>
#include <ipmitool/ipmi.h>
#include <ipmitool/ipmi_channel.h>
#include <ipmitool/ipmi_intf.h>
#include <ipmitool/ipmi_mc.h>
#include <ipmitool/ipmi_pef.h>
#include <ipmitool/ipmi_sel.h>
#include <ipmitool/ipmi_time.h>
#include <ipmitool/log.h>

extern int verbose;
/*
// common kywd/value printf() templates
*/
static const char * pef_fld_fmts[][2] = {
	{"%-*s : %u\n",          " | %u"},			/* F_DEC: unsigned value */
	{"%-*s : %d\n",          " | %d"},			/* F_INT: signed value   */
	{"%-*s : %s\n",          " | %s"},			/* F_STR: string value   */
	{"%-*s : 0x%x\n",        " | 0x%x"},		/* F_HEX: "N hex digits" */
	{"%-*s : 0x%04x\n",      " | 0x%04x"},		/* F_2XD: "2 hex digits" */
	{"%-*s : 0x%02x\n",      " | 0x%02x"},		/* F_1XD: "1 hex digit"  */
	{"%-*s : %02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x\n",
	     " | %02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x"},
};
typedef enum {
	F_DEC,
	F_INT,
	F_STR,
	F_HEX,
	F_2XD,
	F_1XD,
	F_UID,
} fmt_e;
#define KYWD_LENGTH 24
static int first_field = 1;

static const char * pef_flag_fmts[][3] = {
	{"",          "false",  "true"},
	{"supported", "un",         ""},
	{"active",    "in",         ""},
	{"abled",     "dis",      "en"},
};
static const char * listitem[] =	{" | %s", ",%s", "%s"};

static struct bit_desc_map
pef_b2s_actions = {
BIT_DESC_MAP_ALL,
{	{"Alert",			PEF_ACTION_ALERT},
	{"Power-off",			PEF_ACTION_POWER_DOWN},
	{"Reset",			PEF_ACTION_RESET},
	{"Power-cycle",			PEF_ACTION_POWER_CYCLE},
	{"OEM-defined",			PEF_ACTION_OEM},
	{"Diagnostic-interrupt",	PEF_ACTION_DIAGNOSTIC_INTERRUPT},
	{NULL}
}	};

static struct bit_desc_map
pef_b2s_severities = {
BIT_DESC_MAP_ANY,
{	{"Non-recoverable",	PEF_SEVERITY_NON_RECOVERABLE},
	{"Critical",		PEF_SEVERITY_CRITICAL},
	{"Warning",		PEF_SEVERITY_WARNING},
	{"OK",			PEF_SEVERITY_OK},
	{"Information",		PEF_SEVERITY_INFORMATION},
	{"Monitor",		PEF_SEVERITY_MONITOR},
	{NULL}
}	};

static struct bit_desc_map
pef_b2s_sensortypes = {
BIT_DESC_MAP_LIST,
{	{"Any",					255},
	{"Temperature",				1},
	{"Voltage",				2},
	{"Current",				3},
	{"Fan",					4},
	{"Chassis Intrusion",			5},
	{"Platform security breach",		6},
	{"Processor",				7},
	{"Power supply",			8},
	{"Power Unit",				9},
	{"Cooling device",			10},
	{"Other (units-based)",			11},
	{"Memory",				12},
	{"Drive Slot",				13},
	{"POST memory resize",			14},
	{"POST error",				15},
	{"Logging disabled",			16},
	{"Watchdog 1",				17},
	{"System event",			18},
	{"Critical Interrupt",			19},
	{"Button",				20},
	{"Module/board",			21},
	{"uController/coprocessor",		22},
	{"Add-in card",				23},
	{"Chassis",				24},
	{"Chipset",				25},
	{"Other (FRU)",				26},
	{"Cable/interconnect",			27},
	{"Terminator",				28},
	{"System boot",				29},
	{"Boot error",				30},
	{"OS boot",				31},
	{"OS critical stop",			32},
	{"Slot/connector",			33},
	{"ACPI power state",			34},
	{"Watchdog 2",				35},
	{"Platform alert",			36},
	{"Entity presence",			37},
	{"Monitor ASIC/IC",			38},
	{"LAN",					39},
	{"Management subsystem health",		40},
	{"Battery",				41},
	{NULL}
}	};

static struct bit_desc_map
pef_b2s_gentype_1 = {
BIT_DESC_MAP_LIST,
{	{"<LNC",	0},		/* '<' : getting worse */
	{">LNC",	1},		/* '>' : getting better */
	{"<LC",		2},
	{">LC",		3},
	{"<LNR",	4},
	{">LNR",	5},
	{">UNC",	6},
	{"<UNC",	7},
	{">UC",		8},
	{"<UC",		9},
	{">UNR",	10},
	{"<UNR",	11},
	{NULL}
}	};

static struct bit_desc_map
pef_b2s_gentype_2 = {
BIT_DESC_MAP_LIST,
{	{"transition to idle",		0},
	{"transition to active",	1},
	{"transition to busy",		2},
	{NULL}
}	};

static struct bit_desc_map
pef_b2s_gentype_3 = {
BIT_DESC_MAP_LIST,
{	{"state deasserted",	0},
	{"state asserted",	1},
	{NULL}
}	};

static struct bit_desc_map
pef_b2s_gentype_4 = {
BIT_DESC_MAP_LIST,
{	{"predictive failure deasserted",	0},
	{"predictive failure asserted",		1},
	{NULL}
}	};

static struct bit_desc_map
pef_b2s_gentype_5 = {
BIT_DESC_MAP_LIST,
{	{"limit not exceeded",	0},
	{"limit exceeded",	1},
	{NULL}
}	};

static struct bit_desc_map
pef_b2s_gentype_6 = {
BIT_DESC_MAP_LIST,
{	{"performance met",	0},
	{"performance lags",	1},
	{NULL}
}	};

static struct bit_desc_map
pef_b2s_gentype_7 = {
BIT_DESC_MAP_LIST,
{	{"ok",			0},
	{"<warn",		1},		/* '<' : getting worse */
	{"<fail",		2},
	{"<dead",		3},
	{">warn",		4},		/* '>' : getting better */
	{">fail",		5},
	{"dead",		6},
	{"monitor",		7},
	{"informational",	8},
	{NULL}
}	};

static struct bit_desc_map
pef_b2s_gentype_8 = {
BIT_DESC_MAP_LIST,
{	{"device removed/absent",	0},
	{"device inserted/present",	1},
	{NULL}
}	};

static struct bit_desc_map
pef_b2s_gentype_9 = {
BIT_DESC_MAP_LIST,
{	{"device disabled",	0},
	{"device enabled",	1},
	{NULL}
}	};

static struct bit_desc_map
pef_b2s_gentype_10 = {
BIT_DESC_MAP_LIST,
{	{"transition to running",	0},
	{"transition to in test",	1},
	{"transition to power off",	2},
	{"transition to online",	3},
	{"transition to offline",	4},
	{"transition to off duty",	5},
	{"transition to degraded",	6},
	{"transition to power save",	7},
	{"install error",		8},
	{NULL}
}	};

static struct bit_desc_map
pef_b2s_gentype_11 = {
BIT_DESC_MAP_LIST,
{	{"fully redundant",		0},
	{"redundancy lost",		1},
	{"redundancy degraded",		2},
	{"<non-redundant/sufficient",	3},		/* '<' : getting worse */
	{">non-redundant/sufficient",	4},		/* '>' : getting better */
	{"non-redundant/insufficient",	5},
	{"<redundancy degraded",	6},
	{">redundancy degraded",	7},
	{NULL}
}	};

static struct bit_desc_map
pef_b2s_gentype_12 = {
BIT_DESC_MAP_LIST,
{	{"D0 power state",	0},
	{"D1 power state",	1},
	{"D2 power state",	2},
	{"D3 power state",	3},
	{NULL}
}	};

static struct bit_desc_map *
pef_b2s_generic_ER[] = {
	&pef_b2s_gentype_1,
	&pef_b2s_gentype_2,
	&pef_b2s_gentype_3,
	&pef_b2s_gentype_4,
	&pef_b2s_gentype_5,
	&pef_b2s_gentype_6,
	&pef_b2s_gentype_7,
	&pef_b2s_gentype_8,
	&pef_b2s_gentype_9,
	&pef_b2s_gentype_10,
	&pef_b2s_gentype_11,
	&pef_b2s_gentype_12,
};
#define PEF_B2S_GENERIC_ER_ENTRIES ARRAY_SIZE(pef_b2s_generic_ER)

static struct bit_desc_map
pef_b2s_policies = {
BIT_DESC_MAP_LIST,
{	{"Match-always",		PEF_POLICY_FLAGS_MATCH_ALWAYS},
	{"Try-next-entry",		PEF_POLICY_FLAGS_PREV_OK_SKIP},
	{"Try-next-set",		PEF_POLICY_FLAGS_PREV_OK_NEXT_POLICY_SET},
	{"Try-next-channel",		PEF_POLICY_FLAGS_PREV_OK_NEXT_CHANNEL_IN_SET},
	{"Try-next-destination",	PEF_POLICY_FLAGS_PREV_OK_NEXT_DESTINATION_IN_SET},
	{NULL}
}	};

static struct bit_desc_map
pef_b2s_ch_medium = {
#define PEF_CH_MEDIUM_TYPE_IPMB		1
#define PEF_CH_MEDIUM_TYPE_ICMB_10	2
#define PEF_CH_MEDIUM_TYPE_ICMB_09	3
#define PEF_CH_MEDIUM_TYPE_LAN		4
#define PEF_CH_MEDIUM_TYPE_SERIAL	5
#define PEF_CH_MEDIUM_TYPE_XLAN		6
#define PEF_CH_MEDIUM_TYPE_PCI_SMBUS	7
#define PEF_CH_MEDIUM_TYPE_SMBUS_V1X	8
#define PEF_CH_MEDIUM_TYPE_SMBUS_V2X	9
#define PEF_CH_MEDIUM_TYPE_USB_V1X	10
#define PEF_CH_MEDIUM_TYPE_USB_V2X	11
#define PEF_CH_MEDIUM_TYPE_SYSTEM	12
BIT_DESC_MAP_LIST,
{	{"IPMB (I2C)",			PEF_CH_MEDIUM_TYPE_IPMB},
	{"ICMB v1.0",			PEF_CH_MEDIUM_TYPE_ICMB_10},
	{"ICMB v0.9",			PEF_CH_MEDIUM_TYPE_ICMB_09},
	{"802.3 LAN",			PEF_CH_MEDIUM_TYPE_LAN},
	{"Serial/Modem (RS-232)",	PEF_CH_MEDIUM_TYPE_SERIAL},
	{"Other LAN",			PEF_CH_MEDIUM_TYPE_XLAN},
	{"PCI SMBus",			PEF_CH_MEDIUM_TYPE_PCI_SMBUS},
	{"SMBus v1.0/1.1",		PEF_CH_MEDIUM_TYPE_SMBUS_V1X},
	{"SMBus v2.0",			PEF_CH_MEDIUM_TYPE_SMBUS_V2X},
	{"USB 1.x",			PEF_CH_MEDIUM_TYPE_USB_V1X},
	{"USB 2.x",			PEF_CH_MEDIUM_TYPE_USB_V2X},
	{"System I/F (KCS,SMIC,BT)",	PEF_CH_MEDIUM_TYPE_SYSTEM},
	{NULL}
}	};

static struct bit_desc_map
pef_b2s_control = {
BIT_DESC_MAP_ALL,
{	{"PEF",				PEF_CONTROL_ENABLE},
	{"PEF event messages",		PEF_CONTROL_ENABLE_EVENT_MESSAGES},
	{"PEF startup delay",		PEF_CONTROL_ENABLE_STARTUP_DELAY},
	{"Alert startup delay",		PEF_CONTROL_ENABLE_ALERT_STARTUP_DELAY},
	{NULL}
}	};

static struct bit_desc_map
pef_b2s_lan_desttype = {
BIT_DESC_MAP_LIST,
{	{"Acknowledged",	PEF_LAN_DEST_TYPE_ACK},
	{"PET",			PEF_LAN_DEST_TYPE_PET},
	{"OEM 1",		PEF_LAN_DEST_TYPE_OEM_1},
	{"OEM 2",		PEF_LAN_DEST_TYPE_OEM_2},
	{NULL}
}	};

static struct bit_desc_map
pef_b2s_serial_desttype = {
BIT_DESC_MAP_LIST,
{	{"Acknowledged",	PEF_SERIAL_DEST_TYPE_ACK},
	{"TAP page",		PEF_SERIAL_DEST_TYPE_TAP},
	{"PPP PET",		PEF_SERIAL_DEST_TYPE_PPP},
	{"Basic callback",	PEF_SERIAL_DEST_TYPE_BASIC_CALLBACK},
	{"PPP callback",	PEF_SERIAL_DEST_TYPE_PPP_CALLBACK},
	{"OEM 1",		PEF_SERIAL_DEST_TYPE_OEM_1},
	{"OEM 2",		PEF_SERIAL_DEST_TYPE_OEM_2},
	{NULL}
}	};

static struct bit_desc_map
pef_b2s_tap_svc_confirm = {
BIT_DESC_MAP_LIST,
{	{"ACK",			PEF_SERIAL_TAP_CONFIRMATION_ACK_AFTER_ETX},
	{"211+ACK",		PEF_SERIAL_TAP_CONFIRMATION_211_ACK_AFTER_ETX},
	{"{211|213}+ACK",	PEF_SERIAL_TAP_CONFIRMATION_21X_ACK_AFTER_ETX},
	{NULL}
}	};

static int ipmi_pef2_list_filters(struct ipmi_intf *);

const char * 
ipmi_pef_bit_desc(struct bit_desc_map * map, uint32_t value)
{	/*
	// return description/text label(s) for the given value.
	//  NB: uses a static buffer
	*/
	static char buf[128];
	char * p;
	struct desc_map * pmap;
	uint32_t match, index;
	
	*(p = buf) = '\0';
	index = 2;
	for (pmap=map->desc_maps; pmap && pmap->desc; pmap++) {
		if (map->desc_map_type == BIT_DESC_MAP_LIST)
			match = (value == pmap->mask);
		else
			match = ((value & pmap->mask) == pmap->mask);

		if (match) {
			sprintf(p, listitem[index], pmap->desc);
			p = strchr(p, '\0');
			if (map->desc_map_type != BIT_DESC_MAP_ALL)
				break;
			index = 1;
		}
	}
	if (p == buf)
		return("None");

	return((const char *)buf);
}

void
ipmi_pef_print_flags(struct bit_desc_map * map, flg_e type, uint32_t val)
{	/*
	// print features/flags, using val (a bitmask), according to map.
	// observe the verbose flag, and print any labels, etc. based on type
	*/
	struct desc_map * pmap;
	uint32_t maskval, index;

	index = 0;
	for (pmap=map->desc_maps; pmap && pmap->desc; pmap++) {
		maskval = (val & pmap->mask);
		if (verbose)
			printf("%-*s : %s%s\n", KYWD_LENGTH, 
				ipmi_pef_bit_desc(map, pmap->mask),
				pef_flag_fmts[type][1 + (maskval != 0)],
				pef_flag_fmts[type][0]);
		else if (maskval != 0) {
			printf(listitem[index], ipmi_pef_bit_desc(map, maskval));
			index = 1;
		}
	}
}

static void
ipmi_pef_print_field(const char * fmt[2], const char * label, unsigned long val)
{	/*
	// print a 'field' (observes 'verbose' flag)
	*/
	if (verbose)
		printf(fmt[0], KYWD_LENGTH, label, val);
	else if (first_field)
		printf(&fmt[1][2], val);	/* skip field separator */
	else
		printf(fmt[1], val);

	first_field = 0;
}

void
ipmi_pef_print_dec(const char * text, uint32_t val)
{	/* unsigned */
	ipmi_pef_print_field(pef_fld_fmts[F_DEC], text, val);
}

void
ipmi_pef_print_int(const char * text, uint32_t val)
{	/* signed */
	ipmi_pef_print_field(pef_fld_fmts[F_INT], text, val);
}

void
ipmi_pef_print_hex(const char * text, uint32_t val)
{	/* hex */
	ipmi_pef_print_field(pef_fld_fmts[F_HEX], text, val);
}

void 
ipmi_pef_print_str(const char * text, const char * val)
{	/* string */
	ipmi_pef_print_field(pef_fld_fmts[F_STR], text, (unsigned long)val);
}

void 
ipmi_pef_print_2xd(const char * text, uint8_t u1, uint8_t u2)
{	/* 2 hex digits */
	uint32_t val = ((u1 << 8) + u2) & 0xffff;
	ipmi_pef_print_field(pef_fld_fmts[F_2XD], text, val);
}

void 
ipmi_pef_print_1xd(const char * text, uint32_t val)
{	/* 1 hex digit */
	ipmi_pef_print_field(pef_fld_fmts[F_1XD], text, val);
}

/* ipmi_pef_print_guid - print-out GUID. */
static int
ipmi_pef_print_guid(uint8_t *guid)
{
	if (!guid) {
		return (-1);
	}

	if (verbose) {
		printf("%-*s : %02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x\n",
				KYWD_LENGTH, "System GUID",
				guid[0], guid[1], guid[2], guid[3], guid[4],
				guid[5], guid[6], guid[7], guid[8], guid[9],
				guid[10],guid[11], guid[12], guid[13], guid[14],
				guid[15]);
	} else {
		printf(" | %02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",
				guid[0], guid[1], guid[2], guid[3], guid[4],
				guid[5], guid[6], guid[7], guid[8], guid[9],
				guid[10], guid[11], guid[12], guid[13], guid[14],
				guid[15]);
	}
	return 0;
}

static struct ipmi_rs * 
ipmi_pef_msg_exchange(struct ipmi_intf * intf, struct ipmi_rq * req, char * txt)
{	/*
	// common IPMItool rqst/resp handling
	*/
	struct ipmi_rs * rsp = intf->sendrecv(intf, req);
	if (!rsp) {
		return(NULL);
	} else if (rsp->ccode == 0x80)	{
		return(NULL);   /* Do not output error, just unsupported parameters */
	} else if (rsp->ccode) {
		lprintf(LOG_ERR, " **Error %x in '%s' command", rsp->ccode, txt);
		return(NULL);
	}
	if (verbose > 2) {
		printbuf(rsp->data, rsp->data_len, txt);
	}
	return(rsp);
}

/* _ipmi_get_pef_capabilities - Requests and returns result of (30.1) Get PEF
 * Capabilities.
 *
 * @pcap - pointer where to store results.
 *
 * returns - negative number means error, positive is a ccode.
 */
int
_ipmi_get_pef_capabilities(struct ipmi_intf *intf,
		struct pef_capabilities *pcap)
{
	struct ipmi_rs *rsp;
	struct ipmi_rq req;
	if (!pcap) {
		return (-3);
	}

	memset(pcap, 0, sizeof(struct pef_capabilities));
	memset(&req, 0, sizeof(req));
	req.msg.netfn = IPMI_NETFN_SE;
	req.msg.cmd = IPMI_CMD_GET_PEF_CAPABILITIES;

	rsp = intf->sendrecv(intf, &req);
	if (!rsp) {
		return (-1);
	} else if (rsp->ccode) {
		return rsp->ccode;
	} else if (rsp->data_len != 3) {
		return (-2);
	}
	pcap->version = rsp->data[0];
	pcap->actions = rsp->data[1];
	pcap->event_filter_count = rsp->data[2];
	return 0;
}

/* _ipmi_get_pef_filter_entry - Fetches one Entry from Event Filter Table
 * identified by Filter ID.
 *
 * @filter_id - Filter ID of Entry in Event Filter Table.
 * @filter_entry - Pointer where to copy Filter Entry data.
 *
 * returns - negative number means error, positive is a ccode.
 */
static int
_ipmi_get_pef_filter_entry(struct ipmi_intf *intf, uint8_t filter_id,
		struct pef_cfgparm_filter_table_entry *filter_entry)
{
	struct ipmi_rs *rsp;
	struct ipmi_rq req;
	uint8_t data[3];
	uint8_t data_len = 3 * sizeof(uint8_t);
	int dest_size;
	if (!filter_entry) {
		return (-3);
	}

	dest_size = (int)sizeof(struct pef_cfgparm_filter_table_entry);
	memset(filter_entry, 0, dest_size);
	memset(&data, 0, data_len);
	data[0] = PEF_CFGPARM_ID_PEF_FILTER_TABLE_ENTRY;
	data[1] = filter_id;

	memset(&req, 0, sizeof(req));
	req.msg.netfn = IPMI_NETFN_SE;
	req.msg.cmd = IPMI_CMD_GET_PEF_CONFIG_PARMS;
	req.msg.data = (uint8_t *)&data;
	req.msg.data_len = data_len;
	rsp = intf->sendrecv(intf, &req);
	if (!rsp) {
		return (-1);
	} else if (rsp->ccode) {
		return rsp->ccode;
	} else if (rsp->data_len != 22 || (rsp->data_len - 1) != dest_size) {
		return (-2);
	}
	memcpy(filter_entry, &rsp->data[1], dest_size);
	return 0;
}

/* _ipmi_get_pef_filter_entry_cfg - Fetches configuration of one Entry from
 * Event Filter Table identified by Filter ID.
 *
 * @filter_id - Filter ID of Entry in Event Filter Table.
 * @filter_entry_cfg - Pointer where to copy Filter Entry configuration.
 *
 * returns - negative number means error, positive is a ccode.
 */
int
_ipmi_get_pef_filter_entry_cfg(struct ipmi_intf *intf, uint8_t filter_id,
		struct pef_cfgparm_filter_table_data_1 *filter_cfg)
{
	struct ipmi_rs *rsp;
	struct ipmi_rq req;
	uint8_t data[3];
	uint8_t data_len = 3 * sizeof(uint8_t);
	int dest_size;
	if (!filter_cfg) {
		return (-3);
	}

	dest_size = (int)sizeof(struct pef_cfgparm_filter_table_data_1);
	memset(filter_cfg, 0, dest_size);
	memset(&data, 0, data_len);
	data[0] = PEF_CFGPARM_ID_PEF_FILTER_TABLE_DATA_1;
	data[1] = filter_id;

	memset(&req, 0, sizeof(req));
	req.msg.netfn = IPMI_NETFN_SE;
	req.msg.cmd = IPMI_CMD_GET_PEF_CONFIG_PARMS;
	req.msg.data = (uint8_t *)&data;
	req.msg.data_len = data_len;
	rsp = intf->sendrecv(intf, &req);
	if (!rsp) {
		return (-1);
	} else if (rsp->ccode) {
		return rsp->ccode;
	} else if (rsp->data_len != 3 || (rsp->data_len - 1) != dest_size) {
		return (-2);
	}
	memcpy(filter_cfg, &rsp->data[1], dest_size);
	return 0;
}

/* _ipmi_get_pef_policy_entry - Fetches one Entry from Alert Policy Table
 * identified by Policy ID.
 *
 * @policy_id - Policy ID of Entry in Alert Policy Table.
 * @policy_entry - Pointer where to copy Policy Entry data.
 *
 * returns - negative number means error, positive is a ccode.
 */
static int
_ipmi_get_pef_policy_entry(struct ipmi_intf *intf, uint8_t policy_id,
		struct pef_cfgparm_policy_table_entry *policy_entry)
{
	struct ipmi_rs *rsp;
	struct ipmi_rq req;
	uint8_t data[3];
	uint8_t data_len = 3 * sizeof(uint8_t);
	int dest_size;
	if (!policy_entry) {
		return (-3);
	}

	dest_size = (int)sizeof(struct pef_cfgparm_policy_table_entry);
	memset(policy_entry, 0, dest_size);
	memset(&data, 0, data_len);
	data[0] = PEF_CFGPARM_ID_PEF_ALERT_POLICY_TABLE_ENTRY;
	data[1] = policy_id & PEF_POLICY_TABLE_ID_MASK;

	memset(&req, 0, sizeof(req));
	req.msg.netfn = IPMI_NETFN_SE;
	req.msg.cmd = IPMI_CMD_GET_PEF_CONFIG_PARMS;
	req.msg.data = (uint8_t *)&data;
	req.msg.data_len = data_len;
	rsp = intf->sendrecv(intf, &req);
	if (!rsp) {
		return (-1);
	} else if (rsp->ccode) {
		return rsp->ccode;
	} else if (rsp->data_len != 5 || (rsp->data_len - 1) != dest_size) {
		return (-2);
	}
	memcpy(policy_entry, &rsp->data[1], dest_size);
	return 0;
}

/* _ipmi_get_pef_filter_table_size - Fetch the Number of Event Filter Entries.
 * If the number is 0, it means feature is not supported.
 *
 * @table_size - ptr to where to store number of entries.
 *
 * returns - negative number means error, positive is a ccode.
 */
static int
_ipmi_get_pef_filter_table_size(struct ipmi_intf *intf, uint8_t *table_size)
{
	struct ipmi_rs *rsp;
	struct ipmi_rq req;
	struct pef_cfgparm_selector psel;

	if (!table_size) {
		return (-3);
	}

	*table_size = 0;
	memset(&psel, 0, sizeof(psel));
	psel.id = PEF_CFGPARM_ID_PEF_FILTER_TABLE_SIZE;
	memset(&req, 0, sizeof(req));

	req.msg.netfn = IPMI_NETFN_SE;
	req.msg.cmd = IPMI_CMD_GET_PEF_CONFIG_PARMS;
	req.msg.data = (uint8_t *)&psel;
	req.msg.data_len = sizeof(psel);
	rsp = intf->sendrecv(intf, &req);
	if (!rsp) {
		return (-1);
	} else if (rsp->ccode) {
		return rsp->ccode;
	} else if (rsp->data_len != 2) {
		return (-2);
	}
	*table_size = rsp->data[1] & 0x7F;
	return 0;
}

/* _ipmi_get_pef_policy_table_size - Fetch the Number of Alert Policy Entries. If the
 * number is 0, it means feature is not supported.
 *
 * @table_size - ptr to where to store number of entries.
 *
 * returns - negative number means error, positive is a ccode.
 */
static int
_ipmi_get_pef_policy_table_size(struct ipmi_intf *intf, uint8_t *table_size)
{
	struct ipmi_rs *rsp;
	struct ipmi_rq req;
	struct pef_cfgparm_selector psel;

	if (!table_size) {
		return (-3);
	}

	*table_size = 0;
	memset(&psel, 0, sizeof(psel));
	psel.id = PEF_CFGPARM_ID_PEF_ALERT_POLICY_TABLE_SIZE;
	memset(&req, 0, sizeof(req));

	req.msg.netfn = IPMI_NETFN_SE;
	req.msg.cmd = IPMI_CMD_GET_PEF_CONFIG_PARMS;
	req.msg.data = (uint8_t *)&psel;
	req.msg.data_len = sizeof(psel);
	rsp = intf->sendrecv(intf, &req);
	if (!rsp) {
		return (-1);
	} else if (rsp->ccode) {
		return rsp->ccode;
	} else if (rsp->data_len != 2) {
		return (-2);
	}
	*table_size = rsp->data[1] & 0x7F;
	return 0;
}

/* _ipmi_get_pef_system_guid - Fetches System GUID from PEF. This configuration
 * parameter is optional. If data1 is 0x0, then this GUID is ignored by BMC.
 *
 * @system_guid - pointer where to store received data.
 *
 * returns - negative number means error, positive is a ccode.
 */
int
_ipmi_get_pef_system_guid(struct ipmi_intf *intf,
		struct pef_cfgparm_system_guid *system_guid)
{
	struct ipmi_rs *rsp;
	struct ipmi_rq req;
	struct pef_cfgparm_selector psel;
	if (!system_guid) {
		return (-3);
	}

	memset(system_guid, 0, sizeof(struct pef_cfgparm_system_guid));
	memset(&psel, 0, sizeof(psel));
	psel.id = PEF_CFGPARM_ID_SYSTEM_GUID;
	memset(&req, 0, sizeof(req));
	req.msg.netfn = IPMI_NETFN_SE;
	req.msg.cmd = IPMI_CMD_GET_PEF_CONFIG_PARMS;
	req.msg.data = (uint8_t *)&psel;
	req.msg.data_len = sizeof(psel);

	rsp = intf->sendrecv(intf, &req);
	if (!rsp) {
		return (-1);
	} else if (rsp->ccode) {
		return rsp->ccode;
	} else if (rsp->data_len != 18
			|| (rsp->data_len - 2) != sizeof(system_guid->guid)) {
		return (-2);
	}
	system_guid->data1 = rsp->data[1] & 0x1;
	memcpy(system_guid->guid, &rsp->data[2], sizeof(system_guid->guid));
	return 0;
}

/* _ipmi_set_pef_filter_entry_cfg - Sets/updates configuration of Entry in Event
 * Filter Table identified by Filter ID.
 *
 * @filter_id - ID of Entry in Event Filter Table to be updated
 * @filter_cfg - Pointer to configuration data.
 *
 * returns - negative number means error, positive is a ccode.
 */
static int
_ipmi_set_pef_filter_entry_cfg(struct ipmi_intf *intf, uint8_t filter_id,
		struct pef_cfgparm_filter_table_data_1 *filter_cfg)
{
	struct ipmi_rs *rsp;
	struct ipmi_rq req;
	uint8_t data[3];
	uint8_t data_len = 3 * sizeof(uint8_t);
	if (!filter_cfg) {
		return (-3);
	}

	memset(&req, 0, sizeof(req));
	req.msg.netfn = IPMI_NETFN_SE;
	req.msg.cmd = IPMI_CMD_SET_PEF_CONFIG_PARMS;
	req.msg.data = (uint8_t *)&data;
	req.msg.data_len = data_len;

	memset(&data, 0, data_len);
	data[0] = PEF_CFGPARM_ID_PEF_FILTER_TABLE_DATA_1;
	data[1] = filter_id;
	data[2] = filter_cfg->cfg;

	rsp = intf->sendrecv(intf, &req);
	if (!rsp) {
		return (-1);
	} else if (rsp->ccode) {
		return rsp->ccode;
	}
	return 0;
}

/* _ipmi_set_pef_policy_entry - Sets/updates Entry in Alert Policy Table identified by
 * Policy ID.
 *
 * @policy_id - Policy ID of Entry in Alert Policy Table to be updated
 * @policy_entry - Pointer to data.
 *
 * returns - negative number means error, positive is a ccode.
 */
static int
_ipmi_set_pef_policy_entry(struct ipmi_intf *intf, uint8_t policy_id,
		struct pef_cfgparm_policy_table_entry *policy_entry)
{
	struct ipmi_rs *rsp;
	struct ipmi_rq req;
	struct pef_cfgparm_set_policy_table_entry payload;
	if (!policy_entry) {
		return (-3);
	}

	memset(&req, 0, sizeof(req));
	req.msg.netfn = IPMI_NETFN_SE;
	req.msg.cmd = IPMI_CMD_SET_PEF_CONFIG_PARMS;
	req.msg.data = (uint8_t *)&payload;
	req.msg.data_len = sizeof(payload);

	memset(&payload, 0, sizeof(payload));
	payload.param_selector = PEF_CFGPARM_ID_PEF_ALERT_POLICY_TABLE_ENTRY;
	payload.policy_id = policy_id & PEF_POLICY_TABLE_ID_MASK;
	memcpy(&payload.entry, &policy_entry->entry,
			sizeof(policy_entry->entry));

	rsp = intf->sendrecv(intf, &req);
	if (!rsp) {
		return (-1);
	} else if (rsp->ccode) {
		return rsp->ccode;
	}
	return 0;
}

static void
ipmi_pef_print_oem_lan_dest(struct ipmi_intf *intf,
                            uint8_t dest)
{
	char address[128];
	int len;
	int rc;
	int rlen;
	int set;
	uint8_t data[32];

	if (ipmi_get_oem(intf) != IPMI_OEM_DELL) {
		return;
	}
	/* Get # of IPV6 trap destinations */
	rc = ipmi_mc_getsysinfo(intf, IPMI_SYSINFO_DELL_IPV6_COUNT, 0x00, 0x00, 4, data);
	if (rc != 0 || dest > data[0]) {
		return;
	}
	ipmi_pef_print_str("Alert destination type", "xxx");
	ipmi_pef_print_str("PET Community", "xxx");
	ipmi_pef_print_dec("ACK timeout/retry (secs)", 0);
	ipmi_pef_print_dec("Retries", 0);

	/* Get IPv6 destination string (may be in multiple sets) */
	memset(address, 0, sizeof(address));
	memset(data, 0, sizeof(data));
	rc = ipmi_mc_getsysinfo(intf, IPMI_SYSINFO_DELL_IPV6_DESTADDR, 0x00, dest, 19, data);
	if (rc != 0) {
		return;
	}
	/* Total length of IPv6 string */
	len = data[4];
	if ((rlen = len) > (IPMI_SYSINFO_SET0_SIZE-3)) {
		/* First set has 11 bytes */
		rlen = IPMI_SYSINFO_SET0_SIZE - 3;
	}
	memcpy(address, data + 8, rlen);
	for (set = 1; len > 11; set++) {
		rc = ipmi_mc_getsysinfo(intf, IPMI_SYSINFO_DELL_IPV6_DESTADDR, set, dest, 19, data);
		if ((rlen = len - 11) >= (IPMI_SYSINFO_SETN_SIZE - 2)) {
			/* Remaining sets have 14 bytes */
			rlen = IPMI_SYSINFO_SETN_SIZE - 2;
		}
		memcpy(address + (set * 11), data + 3, rlen);
		len -= rlen+3;
	}
	ipmi_pef_print_str("IPv6 Address", address);
}

/* TODO - rewrite */
static void
ipmi_pef_print_lan_dest(struct ipmi_intf * intf, uint8_t ch, uint8_t dest)
{	/*
	// print LAN alert destination info
	*/
	struct ipmi_rs * rsp;
	struct ipmi_rq req;
	struct pef_lan_cfgparm_selector lsel;
	struct pef_lan_cfgparm_dest_type * ptype;
	struct pef_lan_cfgparm_dest_info * pinfo;
	char buf[32];
	uint8_t dsttype, timeout, retries;

	memset(&lsel, 0, sizeof(lsel));
	lsel.id = PEF_LAN_CFGPARM_ID_DEST_COUNT;
	lsel.ch = ch;
	memset(&req, 0, sizeof(req));
	req.msg.netfn = IPMI_NETFN_TRANSPORT;
	req.msg.cmd = IPMI_CMD_LAN_GET_CONFIG;
	req.msg.data = (uint8_t *)&lsel;
	req.msg.data_len = sizeof(lsel);
	rsp = ipmi_pef_msg_exchange(intf, &req, "Alert destination count");
	if (!rsp) {
		lprintf(LOG_ERR, " **Error retrieving %s",
			"Alert destination count");
		return;
	}

	lsel.id = PEF_LAN_CFGPARM_ID_DESTTYPE;
	lsel.set = dest;
	rsp = ipmi_pef_msg_exchange(intf, &req, "Alert destination type");
	if (!rsp || rsp->data[1] != lsel.set) {
		lprintf(LOG_ERR, " **Error retrieving %s",
			"Alert destination type");
		return;
	}
	ptype = (struct pef_lan_cfgparm_dest_type *)&rsp->data[1];
	dsttype = (ptype->dest_type & PEF_LAN_DEST_TYPE_MASK);
	timeout = ptype->alert_timeout;
	retries = (ptype->retries & PEF_LAN_RETRIES_MASK);
	ipmi_pef_print_str("Alert destination type", 
				ipmi_pef_bit_desc(&pef_b2s_lan_desttype, dsttype));
	if (dsttype == PEF_LAN_DEST_TYPE_PET) {
		lsel.id = PEF_LAN_CFGPARM_ID_PET_COMMUNITY;
		lsel.set = 0;
		rsp = ipmi_pef_msg_exchange(intf, &req, "PET community");
		if (!rsp)
			lprintf(LOG_ERR, " **Error retrieving %s",
				"PET community");
		else {
			rsp->data[19] = '\0';
			ipmi_pef_print_str("PET Community", (const char *)&rsp->data[1]);
		}
	}
	ipmi_pef_print_dec("ACK timeout/retry (secs)", timeout);
	ipmi_pef_print_dec("Retries", retries);

	lsel.id = PEF_LAN_CFGPARM_ID_DESTADDR;
	lsel.set = dest;
	rsp = ipmi_pef_msg_exchange(intf, &req, "Alert destination info");
	if (!rsp || rsp->data[1] != lsel.set)
		lprintf(LOG_ERR, " **Error retrieving %s",
			"Alert destination info");
	else {
		pinfo = (struct pef_lan_cfgparm_dest_info *)&rsp->data[1];
		sprintf(buf, "%u.%u.%u.%u", 
					pinfo->ip[0], pinfo->ip[1], pinfo->ip[2], pinfo->ip[3]);
		ipmi_pef_print_str("IP address", buf);

		ipmi_pef_print_str("MAC address", mac2str(pinfo->mac));
	}
}

static void
ipmi_pef_print_serial_dest_dial(struct ipmi_intf *intf, char *label,
		struct pef_serial_cfgparm_selector *ssel)
{	/*
	// print a dial string
	*/
#define BLOCK_SIZE 16
	struct ipmi_rs * rsp;
	struct ipmi_rq req;
	struct pef_serial_cfgparm_selector tmp;
	char * p, strval[(6 * BLOCK_SIZE) + 1];

	memset(&tmp, 0, sizeof(tmp));
	tmp.id = PEF_SERIAL_CFGPARM_ID_DEST_DIAL_STRING_COUNT;
	memset(&req, 0, sizeof(req));
	req.msg.netfn = IPMI_NETFN_TRANSPORT;
	req.msg.cmd = IPMI_CMD_SERIAL_GET_CONFIG;
	req.msg.data = (uint8_t *)&tmp;
	req.msg.data_len = sizeof(tmp);
	rsp = ipmi_pef_msg_exchange(intf, &req, "Dial string count");
	if (!rsp || (rsp->data[1] & PEF_SERIAL_DIAL_STRING_COUNT_MASK) == 0)
		return;	/* sssh, not supported */

	memcpy(&tmp, ssel, sizeof(tmp));
	tmp.id = PEF_SERIAL_CFGPARM_ID_DEST_DIAL_STRING;
	tmp.block = 1;
	memset(strval, 0, sizeof(strval));
	p = strval;
	for (;;) {
		rsp = ipmi_pef_msg_exchange(intf, &req, label);
		if (!rsp
		|| (rsp->data[1] != ssel->id)
		|| (rsp->data[2] != tmp.block)) {
			lprintf(LOG_ERR, " **Error retrieving %s", label);
			return;
		}
		memcpy(p, &rsp->data[3], BLOCK_SIZE);
		if (strchr(p, '\0') <= (p + BLOCK_SIZE))
			break;
		if ((p += BLOCK_SIZE) >= &strval[sizeof(strval)-1])
			break;
		tmp.block++;
	}

	ipmi_pef_print_str(label, strval);
#undef BLOCK_SIZE
}

static void
ipmi_pef_print_serial_dest_tap(struct ipmi_intf *intf,
		struct pef_serial_cfgparm_selector *ssel)
{	/*
	// print TAP destination info
	*/
	struct ipmi_rs * rsp;
	struct ipmi_rq req;
	struct pef_serial_cfgparm_selector tmp;
	struct pef_serial_cfgparm_tap_svc_settings * pset;
	uint8_t dialstr_id, setting_id;

	memset(&tmp, 0, sizeof(tmp));
	tmp.id = PEF_SERIAL_CFGPARM_ID_TAP_ACCT_COUNT;
	memset(&req, 0, sizeof(req));
	req.msg.netfn = IPMI_NETFN_TRANSPORT;
	req.msg.cmd = IPMI_CMD_SERIAL_GET_CONFIG;
	req.msg.data = (uint8_t *)&tmp;
	req.msg.data_len = sizeof(tmp);
	rsp = ipmi_pef_msg_exchange(intf, &req, "Number of TAP accounts");
	if (!rsp || (rsp->data[1] & PEF_SERIAL_TAP_ACCT_COUNT_MASK) == 0)
		return;	/* sssh, not supported */

	memcpy(&tmp, ssel, sizeof(tmp));
	tmp.id = PEF_SERIAL_CFGPARM_ID_TAP_ACCT_INFO;
	rsp = ipmi_pef_msg_exchange(intf, &req, "TAP account info");
	if (!rsp || (rsp->data[1] != tmp.set)) {
		lprintf(LOG_ERR, " **Error retrieving %s",
			"TAP account info");
		return;
	}
	dialstr_id = (rsp->data[2] & PEF_SERIAL_TAP_ACCT_INFO_DIAL_STRING_ID_MASK);
	dialstr_id >>= PEF_SERIAL_TAP_ACCT_INFO_DIAL_STRING_ID_SHIFT;
	setting_id = (rsp->data[2] & PEF_SERIAL_TAP_ACCT_INFO_SVC_SETTINGS_ID_MASK);
	tmp.set = dialstr_id;
	ipmi_pef_print_serial_dest_dial(intf, "TAP Dial string", &tmp);

	tmp.set = setting_id;
	rsp = ipmi_pef_msg_exchange(intf, &req, "TAP service settings");
	if (!rsp || (rsp->data[1] != tmp.set)) {
		lprintf(LOG_ERR, " **Error retrieving %s",
			"TAP service settings");
		return;
	}
	pset = (struct pef_serial_cfgparm_tap_svc_settings *)&rsp->data[1];
	ipmi_pef_print_str("TAP confirmation",  
		ipmi_pef_bit_desc(&pef_b2s_tap_svc_confirm, pset->confirmation_flags));

	/* TODO : additional TAP settings? */
}

/*
static void
ipmi_pef_print_serial_dest_ppp(struct ipmi_intf *intf,
		struct pef_serial_cfgparm_selector *ssel)
{
}

static void
ipmi_pef_print_serial_dest_callback(struct ipmi_intf *intf,
		struct pef_serial_cfgparm_selector *ssel)
}
*/

static void
ipmi_pef_print_serial_dest(struct ipmi_intf *intf, uint8_t ch, uint8_t dest)
{	/*
	// print Serial/PPP alert destination info
	*/
	struct ipmi_rs * rsp;
	struct ipmi_rq req;
	struct pef_serial_cfgparm_selector ssel;
	uint8_t tbl_size, wrk;
	struct pef_serial_cfgparm_dest_info * pinfo;

	memset(&ssel, 0, sizeof(ssel));
	ssel.id = PEF_SERIAL_CFGPARM_ID_DEST_COUNT;
	ssel.ch = ch;
	memset(&req, 0, sizeof(req));
	req.msg.netfn = IPMI_NETFN_TRANSPORT;
	req.msg.cmd = IPMI_CMD_SERIAL_GET_CONFIG;
	req.msg.data = (uint8_t *)&ssel;
	req.msg.data_len = sizeof(ssel);
	rsp = ipmi_pef_msg_exchange(intf, &req, "Alert destination count");
	if (!rsp) {
		lprintf(LOG_ERR, " **Error retrieving %s",
			"Alert destination count");
		return;
	}
	tbl_size = (rsp->data[1] & PEF_SERIAL_DEST_TABLE_SIZE_MASK);
	if (!dest || tbl_size == 0)	/* Page alerting not supported */
		return;
	if (dest > tbl_size) {
		ipmi_pef_print_oem_lan_dest(intf, dest - tbl_size);
		return;
	}

	ssel.id = PEF_SERIAL_CFGPARM_ID_DESTINFO;
	ssel.set = dest;
	rsp = ipmi_pef_msg_exchange(intf, &req, "Alert destination info");
	if (!rsp || rsp->data[1] != ssel.set)
		lprintf(LOG_ERR, " **Error retrieving %s",
			"Alert destination info");
	else {
		pinfo = (struct pef_serial_cfgparm_dest_info *)rsp->data;
		wrk = (pinfo->dest_type & PEF_SERIAL_DEST_TYPE_MASK);
		ipmi_pef_print_str("Alert destination type", 
					ipmi_pef_bit_desc(&pef_b2s_serial_desttype, wrk));
		ipmi_pef_print_dec("ACK timeout (secs)",
					pinfo->alert_timeout);
		ipmi_pef_print_dec("Retries",
					(pinfo->retries & PEF_SERIAL_RETRIES_MASK));
		switch (wrk) {
			case PEF_SERIAL_DEST_TYPE_DIAL:
				ipmi_pef_print_serial_dest_dial(intf, "Serial dial string", &ssel);
				break;
			case PEF_SERIAL_DEST_TYPE_TAP:
				ipmi_pef_print_serial_dest_tap(intf, &ssel);
				break;
			case PEF_SERIAL_DEST_TYPE_PPP:
				/* ipmi_pef_print_serial_dest_ppp(intf, &ssel); */
				break;
			case PEF_SERIAL_DEST_TYPE_BASIC_CALLBACK:
			case PEF_SERIAL_DEST_TYPE_PPP_CALLBACK:
				/* ipmi_pef_print_serial_dest_callback(intf, &ssel); */
				break;
		}
	}
}

static void
ipmi_pef_print_dest(uint8_t dest)
{	/*
	// print generic alert destination info
	*/
	ipmi_pef_print_dec("Destination ID", dest);
}

void
ipmi_pef_print_event_info(struct pef_cfgparm_filter_table_entry * pef, char * buf)
{	/*
	//  print PEF entry Event info: class, severity, trigger, etc.
	*/
	static char * classes[] = {"Discrete", "Threshold", "OEM"};
	uint16_t offmask;
	char * p;
	unsigned int i;
	uint8_t t;

	ipmi_pef_print_str("Event severity", 
				ipmi_pef_bit_desc(&pef_b2s_severities, pef->entry.severity));

	t = pef->entry.event_trigger;
	if (t == PEF_EVENT_TRIGGER_THRESHOLD)
		i = 1;
	else if (t > PEF_EVENT_TRIGGER_SENSOR_SPECIFIC)
		i = 2;
	else
		i = 0;
	ipmi_pef_print_str("Event class", classes[i]);

	offmask = ((pef->entry.event_data_1_offset_mask[1] << 8)
	          + pef->entry.event_data_1_offset_mask[0]);

	if (offmask == 0xffff || t == PEF_EVENT_TRIGGER_MATCH_ANY)
		strcpy(buf, "Any");
	else if (t == PEF_EVENT_TRIGGER_UNSPECIFIED)
		strcpy(buf, "Unspecified");
	else if (t == PEF_EVENT_TRIGGER_SENSOR_SPECIFIC)
		strcpy(buf, "Sensor-specific");
	else if (t > PEF_EVENT_TRIGGER_SENSOR_SPECIFIC)
		strcpy(buf, "OEM");
	else {
		sprintf(buf, "(0x%02x/0x%04x)", t, offmask);
		p = strchr(buf, '\0');
		for (i=0; i<PEF_B2S_GENERIC_ER_ENTRIES; i++) {
			if (offmask & 1) {
				if ((t-1) >= PEF_B2S_GENERIC_ER_ENTRIES) {
					sprintf(p, ", Unrecognized event trigger");
				} else {
					sprintf(p, ",%s", ipmi_pef_bit_desc(pef_b2s_generic_ER[t-1], i));
				}
				p = strchr(p, '\0');
			}
			offmask >>= 1;
		}
	}

	ipmi_pef_print_str("Event trigger(s)", buf);
}

/* ipmi_pef_print_filter_entry - Print-out Entry of Event Filter Table. */
static void
ipmi_pef_print_filter_entry(struct pef_cfgparm_filter_table_entry *filter_entry)
{
	char buf[128];
	uint8_t filter_enabled;
	uint8_t set;

	ipmi_pef_print_dec("PEF Filter Table entry", filter_entry->data1);

	filter_enabled = filter_entry->entry.config & PEF_CONFIG_ENABLED;
	sprintf(buf, "%sabled", (filter_enabled ? "en" : "dis"));

	switch (filter_entry->entry.config & 0x60) {
	case 0x40:
		strcat(buf, ", pre-configured");
		break;
	case 0x00:
		strcat(buf, ", configurable");
		break;
	default:
		/* Covers 0x60 and 0x20 which are reserved */
		strcat(buf, ", reserved");
		break;
	}
	ipmi_pef_print_str("Status", buf);

	if (!filter_enabled) {
		return;
	}

	ipmi_pef_print_str("Sensor type",
				ipmi_pef_bit_desc(&pef_b2s_sensortypes,
					filter_entry->entry.sensor_type));

	if (filter_entry->entry.sensor_number == PEF_SENSOR_NUMBER_MATCH_ANY) {
		ipmi_pef_print_str("Sensor number", "Any");
	} else {
		ipmi_pef_print_dec("Sensor number",
				filter_entry->entry.sensor_number);
	}

	ipmi_pef_print_event_info(filter_entry, buf);
	ipmi_pef_print_str("Action",
				ipmi_pef_bit_desc(&pef_b2s_actions,
					filter_entry->entry.action));

	if (filter_entry->entry.action & PEF_ACTION_ALERT) {
		set = (filter_entry->entry.policy_number & PEF_POLICY_NUMBER_MASK);
		ipmi_pef_print_int("Policy set", set);
	}
}

/* ipmi_pef2_filter_enable - Enable/Disable specific PEF Event Filter.
 *
 * @enable - enable(1) or disable(0) PEF Event Filter.
 * @filter_id - Filter ID of Entry in Event Filter Table.
 *
 * returns - 0 on success, any other value means error.
 */
static int
ipmi_pef2_filter_enable(struct ipmi_intf *intf, uint8_t enable, uint8_t filter_id)
{
	struct pef_cfgparm_filter_table_data_1 filter_cfg;
	int rc;
	uint8_t filter_table_size;

	rc = _ipmi_get_pef_filter_table_size(intf, &filter_table_size);
	if (eval_ccode(rc) != 0) {
		return (-1);
	} else if (filter_table_size == 0) {
		lprintf(LOG_ERR, "PEF Filter isn't supported.");
		return (-1);
	} else if (filter_id > filter_table_size) {
		lprintf(LOG_ERR,
				"PEF Filter ID out of range. Valid range is (1..%d).",
				filter_table_size);
		return (-1);
	}

	memset(&filter_cfg, 0, sizeof(filter_cfg));
	rc = _ipmi_set_pef_filter_entry_cfg(intf, filter_id, &filter_cfg);
	if (eval_ccode(rc) != 0) {
		return (-1);
	}

	if (enable != 0) {
		/* Enable */
		filter_cfg.cfg |= PEF_FILTER_ENABLED;
	} else {
		/* Disable */
		filter_cfg.cfg &= PEF_FILTER_DISABLED;
	}
	rc = _ipmi_set_pef_filter_entry_cfg(intf, filter_id, &filter_cfg);
	if (eval_ccode(rc) != 0) {
		lprintf(LOG_ERR, "Failed to %s PEF Filter ID %d.",
				enable ? "enable" : "disable",
				filter_id);
		return (-1);
	}
	printf("PEF Filter ID %" PRIu8 " is %s now.\n", filter_id,
			enable ? "enabled" : "disabled");
	return rc;
}

void
ipmi_pef2_filter_help(void)
{
	lprintf(LOG_NOTICE,
"usage: pef filter help");
	lprintf(LOG_NOTICE,
"	pef filter list");
	lprintf(LOG_NOTICE,
"       pef filter enable <id = 1..n>");
	lprintf(LOG_NOTICE,
"       pef filter disable <id = 1..n>");
	lprintf(LOG_NOTICE,
"       pef filter create <id = 1..n> <params>");
	lprintf(LOG_NOTICE,
"       pef filter delete <id = 1..n>");
}

/* ipmi_pef2_filter - Handle processing of "filter" CLI args. */
int
ipmi_pef2_filter(struct ipmi_intf *intf, int argc, char **argv)
{
	int rc = 0;

	if (argc < 1) {
		lprintf(LOG_ERR, "Not enough parameters given.");
		ipmi_pef2_filter_help();
		rc = (-1);
	} else if (!strcmp(argv[0], "help")) {
		ipmi_pef2_filter_help();
		rc = 0;
	} else if (!strcmp(argv[0], "list")) {
		rc = ipmi_pef2_list_filters(intf);
	} else if (!strcmp(argv[0], "enable")
			||(!strcmp(argv[0], "disable"))) {
		uint8_t enable;
		uint8_t filter_id;
		if (argc != 2) {
			lprintf(LOG_ERR, "Not enough arguments given.");
			ipmi_pef2_filter_help();
			return (-1);
		}
		if (str2uchar(argv[1], &filter_id) != 0) {
			lprintf(LOG_ERR, "Invalid PEF Event Filter ID given: %s", argv[1]);
			return (-1);
		} else if (filter_id < 1) {
			lprintf(LOG_ERR, "PEF Event Filter ID out of range. "
					"Valid range is <1..255>.");
			return (-1);
		}
		if (!strcmp(argv[0], "enable")) {
			enable = 1;
		} else {
			enable = 0;
		}
		rc = ipmi_pef2_filter_enable(intf, enable, filter_id);
	} else if (!strcmp(argv[0], "create")) {
		lprintf(LOG_ERR, "Not implemented.");
		rc = 1;
	} else if (!strcmp(argv[0], "delete")) {
		lprintf(LOG_ERR, "Not implemented.");
		rc = 1;
	} else {
		lprintf(LOG_ERR, "Invalid PEF Filter command: %s", argv[0]);
		ipmi_pef2_filter_help();
		rc = 1;
	}
	return rc;
}

/* ipmi_pef2_get_info - Reports PEF capabilities + System GUID */
static int
ipmi_pef2_get_info(struct ipmi_intf *intf)
{
	struct pef_capabilities pcap;
	struct pef_cfgparm_system_guid psys_guid;
	ipmi_guid_t guid;
	int rc;
	uint8_t *guid_ptr = NULL;
	uint8_t policy_table_size;

	rc = _ipmi_get_pef_policy_table_size(intf, &policy_table_size);
	if (eval_ccode(rc) != 0) {
		lprintf(LOG_WARN, "Failed to get size of PEF Policy Table.");
		policy_table_size = 0;
	}
	rc = _ipmi_get_pef_capabilities(intf, &pcap);
	if (eval_ccode(rc) != 0) {
		lprintf(LOG_ERR, "Failed to get PEF Capabilities.");
		return (-1);
	}

	ipmi_pef_print_1xd("Version", pcap.version);
	ipmi_pef_print_dec("PEF Event Filter count",
			pcap.event_filter_count);
	ipmi_pef_print_dec("PEF Alert Policy Table size",
			policy_table_size);

	rc = _ipmi_get_pef_system_guid(intf, &psys_guid);
	if (rc != 0x80 && eval_ccode(rc) != 0) {
		lprintf(LOG_ERR, "Failed to get PEF System GUID. %i", rc);
		return (-1);
	} else if (psys_guid.data1 == 0x1) {
		/* IPMI_CMD_GET_SYSTEM_GUID */
		guid_ptr = &psys_guid.guid[0];
	} else {
		rc = _ipmi_mc_get_guid(intf, &guid);
		if (rc == 0) {
			guid_ptr = (uint8_t *)&guid;
		}
	}
	/* Got GUID? */
	if (guid_ptr) {
		ipmi_pef_print_guid(guid_ptr);
	}
	ipmi_pef_print_flags(&pef_b2s_actions, P_SUPP, pcap.actions);
	putchar('\n');
	return 0;
}

/* ipmi_pef2_get_status - TODO rewrite - report the PEF status */
static int
ipmi_pef2_get_status(struct ipmi_intf *intf)
{
	struct ipmi_rs *rsp;
	struct ipmi_rq req;
	struct pef_cfgparm_selector psel;
	time_t ts;

	memset(&req, 0, sizeof(req));
	req.msg.netfn = IPMI_NETFN_SE;
	req.msg.cmd = IPMI_CMD_GET_LAST_PROCESSED_EVT_ID;
	rsp = ipmi_pef_msg_exchange(intf, &req, "Last S/W processed ID");
	if (!rsp) {
		lprintf(LOG_ERR, " **Error retrieving %s",
			"Last S/W processed ID");
		return (-1);
	}

	ts = ipmi32toh(rsp->data);
	ipmi_pef_print_str("Last SEL addition", ipmi_timestamp_numeric(ts));
	ipmi_pef_print_2xd("Last SEL record ID", rsp->data[5], rsp->data[4]);
	ipmi_pef_print_2xd("Last S/W processed ID", rsp->data[7], rsp->data[6]);
	ipmi_pef_print_2xd("Last BMC processed ID", rsp->data[9], rsp->data[8]);

	memset(&psel, 0, sizeof(psel));
	psel.id = PEF_CFGPARM_ID_PEF_CONTROL;
	memset(&req, 0, sizeof(req));
	req.msg.netfn = IPMI_NETFN_SE;
	req.msg.cmd = IPMI_CMD_GET_PEF_CONFIG_PARMS;
	req.msg.data = (uint8_t *)&psel;
	req.msg.data_len = sizeof(psel);
	rsp = ipmi_pef_msg_exchange(intf, &req, "PEF control");
	if (!rsp) {
		lprintf(LOG_ERR, " **Error retrieving %s",
			"PEF control");
		return (-1);
	}
	ipmi_pef_print_flags(&pef_b2s_control, P_ABLE, rsp->data[1]);

	psel.id = PEF_CFGPARM_ID_PEF_ACTION;
	rsp = ipmi_pef_msg_exchange(intf, &req, "PEF action");
	if (!rsp) {
		lprintf(LOG_ERR, " **Error retrieving %s",
			"PEF action");
		return (-1);
	}
	ipmi_pef_print_flags(&pef_b2s_actions, P_ACTV, rsp->data[1]);
	putchar('\n');
	return 0;
}

/* ipmi_pef2_list_filters - List all entries in PEF Event Filter Table. */
static int
ipmi_pef2_list_filters(struct ipmi_intf *intf)
{
	struct pef_capabilities pcap;
	struct pef_cfgparm_filter_table_entry filter_entry;
	int rc;
	uint8_t i;

	rc = _ipmi_get_pef_capabilities(intf, &pcap);
	if (eval_ccode(rc) != 0) {
		return (-1);
	} else if (pcap.event_filter_count == 0) {
		lprintf(LOG_ERR, "PEF Event Filtering isn't supported.");
		return (-1);
	}

	for (i = 1; i <= pcap.event_filter_count; i++) {
		first_field = 1;
		rc = _ipmi_get_pef_filter_entry(intf, i, &filter_entry);
		if (eval_ccode(rc) != 0) {
			lprintf(LOG_ERR, "Failed to get PEF Event Filter Entry %i.",
					i);
			continue;
		}
		ipmi_pef_print_filter_entry(&filter_entry);
		printf("\n");
	}
	return 0;
}

/* ipmi_pef2_list_policies - List Entries in PEF Alert Policy Table. */
static int
ipmi_pef2_list_policies(struct ipmi_intf *intf)
{
	struct channel_info_t channel_info;
	struct pef_cfgparm_policy_table_entry entry;
	int rc;
	uint8_t dest;
	uint8_t i;
	uint8_t policy_table_size;

	rc = _ipmi_get_pef_policy_table_size(intf, &policy_table_size);
	if (eval_ccode(rc) != 0) {
		return (-1);
	} else if (policy_table_size == 0) {
		lprintf(LOG_ERR, "PEF Alert Policy isn't supported.");
		return (-1);
	}

	for (i = 1; i <= policy_table_size; i++) {
		first_field = 1;
		rc = _ipmi_get_pef_policy_entry(intf, i, &entry);
		if (eval_ccode(rc) != 0) {
			continue;
		}

		ipmi_pef_print_dec("Alert policy table entry",
				   (entry.data1 & PEF_POLICY_TABLE_ID_MASK));
		ipmi_pef_print_dec("Policy set",
				   (entry.entry.policy & PEF_POLICY_ID_MASK) >> PEF_POLICY_ID_SHIFT);
		ipmi_pef_print_str("State",
				   entry.entry.policy & PEF_POLICY_ENABLED ? "enabled" : "disabled");
		ipmi_pef_print_str("Policy entry rule",
				   ipmi_pef_bit_desc(&pef_b2s_policies,
					   (entry.entry.policy & PEF_POLICY_FLAGS_MASK)));

		if (entry.entry.alert_string_key & PEF_POLICY_EVENT_SPECIFIC) {
			ipmi_pef_print_str("Event-specific", "true");
		}
		channel_info.channel = ((entry.entry.chan_dest &
					PEF_POLICY_CHANNEL_MASK) >>
					PEF_POLICY_CHANNEL_SHIFT);
		rc = _ipmi_get_channel_info(intf, &channel_info);
		if (eval_ccode(rc) != 0) {
			continue;
		}
		ipmi_pef_print_dec("Channel number", channel_info.channel);
		ipmi_pef_print_str("Channel medium",
				   ipmi_pef_bit_desc(&pef_b2s_ch_medium,
					   channel_info.medium));
		dest = entry.entry.chan_dest & PEF_POLICY_DESTINATION_MASK;
		switch (channel_info.medium) {
		case PEF_CH_MEDIUM_TYPE_LAN:
			ipmi_pef_print_lan_dest(intf, channel_info.channel,
					dest);
			break;
		case PEF_CH_MEDIUM_TYPE_SERIAL:
			ipmi_pef_print_serial_dest(intf, channel_info.channel,
					dest);
			break;
		default:
			ipmi_pef_print_dest(dest);
			break;
		}
		printf("\n");
	}
	return 0;
}

void
ipmi_pef2_policy_help(void)
{
	lprintf(LOG_NOTICE,
"usage: pef policy help");
	lprintf(LOG_NOTICE,
"       pef policy list");
	lprintf(LOG_NOTICE,
"       pef policy enable <id = 1..n>");
	lprintf(LOG_NOTICE,
"       pef policy disable <id = 1..n>");
	lprintf(LOG_NOTICE,
"       pef policy create <id = 1..n> <params>");
	lprintf(LOG_NOTICE,
"       pef policy delete <id = 1..n>");
}

/* ipmi_pef2_policy_enable - Enable/Disable specific PEF policy
 *
 * @enable - enable(1) or disable(0) PEF Alert Policy
 * @policy_id - Policy ID of Entry in Alert Policy Table.
 *
 * returns - 0 on success, any other value means error.
 */
static int
ipmi_pef2_policy_enable(struct ipmi_intf *intf, int enable, uint8_t policy_id)
{
	struct pef_cfgparm_policy_table_entry policy_entry;
	int rc;
	uint8_t policy_table_size;

	rc = _ipmi_get_pef_policy_table_size(intf, &policy_table_size);
	if (eval_ccode(rc) != 0) {
		return (-1);
	} else if (policy_table_size == 0) {
		lprintf(LOG_ERR, "PEF Policy isn't supported.");
		return (-1);
	} else if (policy_id > policy_table_size) {
		lprintf(LOG_ERR,
				"PEF Policy ID out of range. Valid range is (1..%d).",
				policy_table_size);
		return (-1);
	}

	memset(&policy_entry, 0, sizeof(policy_entry));
	rc = _ipmi_get_pef_policy_entry(intf, policy_id, &policy_entry);
	if (eval_ccode(rc) != 0) {
		return (-1);
	}

	if (enable != 0) {
		/* Enable */
		policy_entry.entry.policy |= PEF_POLICY_ENABLED;
	} else {
		/* Disable */
		policy_entry.entry.policy &= PEF_POLICY_DISABLED;
	}
	rc = _ipmi_set_pef_policy_entry(intf, policy_id, &policy_entry);
	if (eval_ccode(rc) != 0) {
		lprintf(LOG_ERR, "Failed to %s PEF Policy ID %d.",
				enable ? "enable" : "disable",
				policy_id);
		return (-1);
	}
	printf("PEF Policy ID %" PRIu8 " is %s now.\n", policy_id,
			enable ? "enabled" : "disabled");
	return rc;
}

/* ipmi_pef2_policy - Handle processing of "policy" CLI args. */
int
ipmi_pef2_policy(struct ipmi_intf *intf, int argc, char **argv)
{
	int rc = 0;

	if (argc < 1) {
		lprintf(LOG_ERR, "Not enough parameters given.");
		ipmi_pef2_policy_help();
		rc = (-1);
	} else if (!strcmp(argv[0], "help")) {
		ipmi_pef2_policy_help();
		rc = 0;
	} else if (!strcmp(argv[0], "list")) {
		rc = ipmi_pef2_list_policies(intf);
	} else if (!strcmp(argv[0], "enable")
			|| !strcmp(argv[0], "disable")) {
		uint8_t enable;
		uint8_t policy_id;
		if (argc != 2) {
			lprintf(LOG_ERR, "Not enough arguments given.");
			ipmi_pef2_policy_help();
			return (-1);
		}
		if (str2uchar(argv[1], &policy_id) != 0) {
			lprintf(LOG_ERR, "Invalid PEF Policy ID given: %s", argv[1]);
			return (-1);
		} else if (policy_id < 1 || policy_id > 127) {
			lprintf(LOG_ERR, "PEF Policy ID out of range. Valid range is <1..127>.");
			return (-1);
		}
		if (!strcmp(argv[0], "enable")) {
			enable = 1;
		} else {
			enable = 0;
		}
		rc = ipmi_pef2_policy_enable(intf, enable, policy_id);
	} else if (!strcmp(argv[0], "create")) {
		lprintf(LOG_ERR, "Not implemented.");
		rc = 1;
	} else if (!strcmp(argv[0], "delete")) {
		lprintf(LOG_ERR, "Not implemented.");
		rc = 1;
	} else {
		lprintf(LOG_ERR, "Invalid PEF Policy command: %s", argv[0]);
		ipmi_pef2_policy_help();
		rc = 1;
	}
	return rc;
}

/* ipmi_pef2_help - print-out help text. */
void
ipmi_pef2_help(void)
{
	lprintf(LOG_NOTICE,
"usage: pef help");
	lprintf(LOG_NOTICE,
"       pef capabilities");
	lprintf(LOG_NOTICE,
"       pef event <params>");
	lprintf(LOG_NOTICE,
"       pef filter list");
	lprintf(LOG_NOTICE,
"       pef filter enable <id = 1..n>");
	lprintf(LOG_NOTICE,
"       pef filter disable <id = 1..n>");
	lprintf(LOG_NOTICE,
"       pef filter create <id = 1..n> <params>");
	lprintf(LOG_NOTICE,
"       pef filter delete <id = 1..n>");
	lprintf(LOG_NOTICE,
"       pef info");
	lprintf(LOG_NOTICE,
"       pef policy list");
	lprintf(LOG_NOTICE,
"       pef policy enable <id = 1..n>");
	lprintf(LOG_NOTICE,
"       pef policy disable <id = 1..n>");
	lprintf(LOG_NOTICE,
"       pef policy create <id = 1..n> <params>");
	lprintf(LOG_NOTICE,
"       pef policy delete <id = 1..n>");
	lprintf(LOG_NOTICE,
"       pef pet ack <params>");
	lprintf(LOG_NOTICE,
"       pef status");
	lprintf(LOG_NOTICE,
"       pef timer get");
	lprintf(LOG_NOTICE,
"       pef timer set <0x00-0xFF>");
}

int ipmi_pef_main(struct ipmi_intf *intf, int argc, char **argv)
{
	int rc = 0;

	if (argc < 1) {
		lprintf(LOG_ERR, "Not enough parameters given.");
		ipmi_pef2_help();
		rc = (-1);
	} else if (!strcmp(argv[0], "help")) {
		ipmi_pef2_help();
		rc = 0;
	} else if (!strcmp(argv[0], "capabilities")) {
		/* rc = ipmi_pef2_get_capabilities(intf); */
		lprintf(LOG_ERR, "Not implemented.");
		rc = 1;
	} else if (!strcmp(argv[0], "event")) {
		/* rc = ipmi_pef2_event(intf, (argc - 1), ++argv); */
		lprintf(LOG_ERR, "Not implemented.");
		rc = 1;
	} else if (!strcmp(argv[0], "filter")) {
		rc = ipmi_pef2_filter(intf, (argc - 1), ++argv);
	} else if (!strcmp(argv[0], "info")) {
		rc = ipmi_pef2_get_info(intf);
	} else if (!strcmp(argv[0], "pet")) {
		/* rc = ipmi_pef2_pet(intf, (argc - 1), ++argv); */
		lprintf(LOG_ERR, "Not implemented.");
		rc = 1;
	} else if (!strcmp(argv[0], "policy")) {
		rc = ipmi_pef2_policy(intf, (argc - 1), ++argv);
	} else if (!strcmp(argv[0], "status")) {
		rc = ipmi_pef2_get_status(intf);
	} else if (!strcmp(argv[0], "timer")) {
		/* rc = ipmi_pef2_timer(intf, (argc - 1), ++argv); */
		lprintf(LOG_ERR, "Not implemented.");
		rc = 1;
	} else {
		lprintf(LOG_ERR, "Invalid PEF command: '%s'\n", argv[0]);
		rc = (-1);
	}
	return rc;
}
