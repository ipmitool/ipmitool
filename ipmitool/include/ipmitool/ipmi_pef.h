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

#ifndef IPMI_PEF_H
#define IPMI_PEF_H

#include <ipmitool/ipmi.h>

/* PEF */

struct pef_capabilities {		/* "get pef capabilities" response */
	uint8_t version;
	uint8_t actions;						/* mapped by PEF_ACTION_xxx */
	uint8_t tblsize;
};

typedef enum {
	P_TRUE,
	P_SUPP,
	P_ACTV,
	P_ABLE,
} flg_e;

struct pef_table_entry {
#define PEF_CONFIG_ENABLED 0x80
#define PEF_CONFIG_PRECONFIGURED 0x40
	uint8_t config;
#define PEF_ACTION_DIAGNOSTIC_INTERRUPT 0x20
#define PEF_ACTION_OEM 0x10
#define PEF_ACTION_POWER_CYCLE 0x08
#define PEF_ACTION_RESET 0x04
#define PEF_ACTION_POWER_DOWN 0x02
#define PEF_ACTION_ALERT 0x01
	uint8_t action;
#define PEF_POLICY_NUMBER_MASK 0x0f
	uint8_t policy_number;
#define PEF_SEVERITY_NON_RECOVERABLE 0x20
#define PEF_SEVERITY_CRITICAL 0x10
#define PEF_SEVERITY_WARNING 0x08
#define PEF_SEVERITY_OK 0x04
#define PEF_SEVERITY_INFORMATION 0x02
#define PEF_SEVERITY_MONITOR 0x01
	uint8_t severity;
	uint8_t generator_ID_addr;
	uint8_t generator_ID_lun;
	uint8_t sensor_type;
#define PEF_SENSOR_NUMBER_MATCH_ANY 0xff
	uint8_t sensor_number;
#define PEF_EVENT_TRIGGER_UNSPECIFIED 0x0
#define PEF_EVENT_TRIGGER_THRESHOLD 0x1
#define PEF_EVENT_TRIGGER_SENSOR_SPECIFIC 0x6f
#define PEF_EVENT_TRIGGER_MATCH_ANY 0xff
	uint8_t event_trigger;
	uint8_t event_data_1_offset_mask[2];
	uint8_t event_data_1_AND_mask;
	uint8_t event_data_1_compare_1;
	uint8_t event_data_1_compare_2;
	uint8_t event_data_2_AND_mask;
	uint8_t event_data_2_compare_1;
	uint8_t event_data_2_compare_2;
	uint8_t event_data_3_AND_mask;
	uint8_t event_data_3_compare_1;
	uint8_t event_data_3_compare_2;
} __attribute__ ((packed));

struct desc_map {						/* maps a description to a value/mask */
	const char *desc;
	uint32_t mask;
};

struct bit_desc_map {				/* description text container */
#define BIT_DESC_MAP_LIST 0x1		/* index-based text array */
#define BIT_DESC_MAP_ANY 0x2		/* bitwise, but only print 1st one */
#define BIT_DESC_MAP_ALL 0x3		/* bitwise, print them all */
	uint32_t desc_map_type;
	struct desc_map desc_maps[128];
};

static struct bit_desc_map
pef_b2s_actions __attribute__((unused)) = {
BIT_DESC_MAP_ALL,
{	{"Alert",						PEF_ACTION_ALERT},
	{"Power-off",					PEF_ACTION_POWER_DOWN},
	{"Reset",						PEF_ACTION_RESET},
	{"Power-cycle",				PEF_ACTION_POWER_CYCLE},
	{"OEM-defined",				PEF_ACTION_OEM},
	{"Diagnostic-interrupt",	PEF_ACTION_DIAGNOSTIC_INTERRUPT},
	{NULL}
}	};

static struct bit_desc_map
pef_b2s_severities __attribute__((unused)) = {
BIT_DESC_MAP_ANY,
{	{"Non-recoverable",			PEF_SEVERITY_NON_RECOVERABLE},
	{"Critical",					PEF_SEVERITY_CRITICAL},
	{"Warning",						PEF_SEVERITY_WARNING},
	{"OK",							PEF_SEVERITY_OK},
	{"Information",				PEF_SEVERITY_INFORMATION},
	{"Monitor",						PEF_SEVERITY_MONITOR},
	{NULL}
}	};

static struct bit_desc_map
pef_b2s_sensortypes __attribute__((unused)) = {
BIT_DESC_MAP_LIST,
{	{"Any",								255},
	{"Temperature",					1},
	{"Voltage",							2},
	{"Current",							3},
	{"Fan",								4},
	{"Chassis Intrusion",			5},
	{"Platform security breach",	6},
	{"Processor",						7},
	{"Power supply",					8},
	{"Power Unit",						9},
	{"Cooling device",				10},
	{"Other (units-based)",			11},
	{"Memory",							12},
	{"Drive Slot",						13},
	{"POST memory resize",			14},
	{"POST error",						15},
	{"Logging disabled",				16},
	{"Watchdog 1",						17},
	{"System event",					18},
	{"Critical Interrupt",			19},
	{"Button",							20},
	{"Module/board",					21},
	{"uController/coprocessor",	22},
	{"Add-in card",					23},
	{"Chassis",							24},
	{"Chipset",							25},
	{"Other (FRU)",					26},
	{"Cable/interconnect",			27},
	{"Terminator",						28},
	{"System boot",					29},
	{"Boot error",						30},
	{"OS boot",							31},
	{"OS critical stop",				32},
	{"Slot/connector",				33},
	{"ACPI power state",				34},
	{"Watchdog 2",						35},
	{"Platform alert",				36},
	{"Entity presence",				37},
	{"Monitor ASIC/IC",				38},
	{"LAN",								39},
	{"Management subsytem health",40},
	{"Battery",							41},
	{NULL}
}	};

static struct bit_desc_map
pef_b2s_gentype_1 = {
BIT_DESC_MAP_LIST,
{	{"<LNC",								0},		/* '<' : getting worse */
	{">LNC",								1},		/* '>' : getting better */
	{"<LC",								2},
	{">LC",								3},
	{"<LNR",								4},
	{">LNR",								5},
	{">UNC",								6},
	{"<UNC",								7},
	{">UC",								8},
	{"<UC",								9},
	{">UNR",								10},
	{"<UNR",								11},
	{NULL}
}	};

static struct bit_desc_map
pef_b2s_gentype_2 = {
BIT_DESC_MAP_LIST,
{	{"transition to idle",			0},
	{"transition to active",		1},
	{"transition to busy",			2},
	{NULL}
}	};

static struct bit_desc_map
pef_b2s_gentype_3 = {
BIT_DESC_MAP_LIST,
{	{"state deasserted",				0},
	{"state asserted",				1},
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
{	{"limit not exceeded",			0},
	{"limit exceeded",				1},
	{NULL}
}	};

static struct bit_desc_map
pef_b2s_gentype_6 = {
BIT_DESC_MAP_LIST,
{	{"performance met",				0},
	{"performance lags",				1},
	{NULL}
}	};

static struct bit_desc_map
pef_b2s_gentype_7 = {
BIT_DESC_MAP_LIST,
{	{"ok",								0},
	{"<warn",							1},		/* '<' : getting worse */
	{"<fail",							2},
	{"<dead",							3},
	{">warn",							4},		/* '>' : getting better */
	{">fail",							5},
	{"dead",								6},
	{"monitor",							7},
	{"informational",					8},
	{NULL}
}	};

static struct bit_desc_map
pef_b2s_gentype_8 = {
BIT_DESC_MAP_LIST,
{	{"device removed/absent",		0},
	{"device inserted/present",	1},
	{NULL}
}	};

static struct bit_desc_map
pef_b2s_gentype_9 = {
BIT_DESC_MAP_LIST,
{	{"device disabled",				0},
	{"device enabled",				1},
	{NULL}
}	};

static struct bit_desc_map
pef_b2s_gentype_10 = {
BIT_DESC_MAP_LIST,
{	{"transition to running",		0},
	{"transition to in test",		1},
	{"transition to power off",	2},
	{"transition to online",		3},
	{"transition to offline",		4},
	{"transition to off duty",		5},
	{"transition to degraded",		6},
	{"transition to power save",	7},
	{"install error",					8},
	{NULL}
}	};

static struct bit_desc_map
pef_b2s_gentype_11 = {
BIT_DESC_MAP_LIST,
{	{"fully redundant",					0},
	{"redundancy lost",					1},
	{"redundancy degraded",				2},
	{"<non-redundant/sufficient",		3},		/* '<' : getting worse */
	{">non-redundant/sufficient",		4},		/* '>' : getting better */
	{"non-redundant/insufficient",	5},
	{"<redundancy degraded",			6},
	{">redundancy degraded",			7},
	{NULL}
}	};

static struct bit_desc_map
pef_b2s_gentype_12 = {
BIT_DESC_MAP_LIST,
{	{"D0 power state",				0},
	{"D1 power state",				1},
	{"D2 power state",				2},
	{"D3 power state",				3},
	{NULL}
}	};

static struct bit_desc_map *
pef_b2s_generic_ER[] __attribute__((unused)) = {
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
#define PEF_B2S_GENERIC_ER_ENTRIES \
			(sizeof(pef_b2s_generic_ER) / sizeof(pef_b2s_generic_ER[0]))

struct pef_policy_entry {
#define PEF_POLICY_ID_MASK 0xf0
#define PEF_POLICY_ID_SHIFT 4
#define PEF_POLICY_ENABLED 0x08
#define PEF_POLICY_FLAGS_MASK 0x07
#define PEF_POLICY_FLAGS_MATCH_ALWAYS 0
#define PEF_POLICY_FLAGS_PREV_OK_SKIP 1
#define PEF_POLICY_FLAGS_PREV_OK_NEXT_POLICY_SET 2
#define PEF_POLICY_FLAGS_PREV_OK_NEXT_CHANNEL_IN_SET 3
#define PEF_POLICY_FLAGS_PREV_OK_NEXT_DESTINATION_IN_SET 4
	uint8_t policy;
#define PEF_POLICY_CHANNEL_MASK 0xf0
#define PEF_POLICY_CHANNEL_SHIFT 4
#define PEF_POLICY_DESTINATION_MASK 0x0f
	uint8_t chan_dest;
#define PEF_POLICY_EVENT_SPECIFIC 0x80
	uint8_t alert_string_key;
} __attribute__ ((packed));

static struct bit_desc_map
pef_b2s_policies __attribute__((unused)) = {
BIT_DESC_MAP_LIST,
{	{"Match-always",				PEF_POLICY_FLAGS_MATCH_ALWAYS},
	{"Try-next-entry",			PEF_POLICY_FLAGS_PREV_OK_SKIP},
	{"Try-next-set",				PEF_POLICY_FLAGS_PREV_OK_NEXT_POLICY_SET},
	{"Try-next-channel",			PEF_POLICY_FLAGS_PREV_OK_NEXT_CHANNEL_IN_SET},
	{"Try-next-destination",	PEF_POLICY_FLAGS_PREV_OK_NEXT_DESTINATION_IN_SET},
	{NULL}
}	};

static struct bit_desc_map
pef_b2s_ch_medium __attribute__((unused)) = {
#define PEF_CH_MEDIUM_TYPE_IPMB			1
#define PEF_CH_MEDIUM_TYPE_ICMB_10		2
#define PEF_CH_MEDIUM_TYPE_ICMB_09		3
#define PEF_CH_MEDIUM_TYPE_LAN			4
#define PEF_CH_MEDIUM_TYPE_SERIAL		5
#define PEF_CH_MEDIUM_TYPE_XLAN			6
#define PEF_CH_MEDIUM_TYPE_PCI_SMBUS	7
#define PEF_CH_MEDIUM_TYPE_SMBUS_V1X	8
#define PEF_CH_MEDIUM_TYPE_SMBUS_V2X	9
#define PEF_CH_MEDIUM_TYPE_USB_V1X		10
#define PEF_CH_MEDIUM_TYPE_USB_V2X		11
#define PEF_CH_MEDIUM_TYPE_SYSTEM		12
BIT_DESC_MAP_LIST,
{	{"IPMB (I2C)",								PEF_CH_MEDIUM_TYPE_IPMB},
	{"ICMB v1.0",								PEF_CH_MEDIUM_TYPE_ICMB_10},
	{"ICMB v0.9",								PEF_CH_MEDIUM_TYPE_ICMB_09},
	{"802.3 LAN",								PEF_CH_MEDIUM_TYPE_LAN},
	{"Serial/Modem (RS-232)",				PEF_CH_MEDIUM_TYPE_SERIAL},
	{"Other LAN",								PEF_CH_MEDIUM_TYPE_XLAN},
	{"PCI SMBus",								PEF_CH_MEDIUM_TYPE_PCI_SMBUS},
	{"SMBus v1.0/1.1",						PEF_CH_MEDIUM_TYPE_SMBUS_V1X},
	{"SMBus v2.0",								PEF_CH_MEDIUM_TYPE_SMBUS_V2X},
	{"USB 1.x",									PEF_CH_MEDIUM_TYPE_USB_V1X},
	{"USB 2.x",									PEF_CH_MEDIUM_TYPE_USB_V2X},
	{"System I/F (KCS,SMIC,BT)",			PEF_CH_MEDIUM_TYPE_SYSTEM},
	{NULL}
}	};

struct pef_cfgparm_selector {
#define PEF_CFGPARM_ID_REVISION_ONLY_MASK 0x80
#define PEF_CFGPARM_ID_SET_IN_PROGRESS 0
#define PEF_CFGPARM_ID_PEF_CONTROL 1
#define PEF_CFGPARM_ID_PEF_ACTION 2
#define PEF_CFGPARM_ID_PEF_STARTUP_DELAY 3
#define PEF_CFGPARM_ID_PEF_ALERT_STARTUP_DELAY 4
#define PEF_CFGPARM_ID_PEF_FILTER_TABLE_SIZE 5
#define PEF_CFGPARM_ID_PEF_FILTER_TABLE_ENTRY 6
#define PEF_CFGPARM_ID_PEF_FILTER_TABLE_DATA_1 7
#define PEF_CFGPARM_ID_PEF_ALERT_POLICY_TABLE_SIZE 8
#define PEF_CFGPARM_ID_PEF_ALERT_POLICY_TABLE_ENTRY 9
#define PEF_CFGPARM_ID_SYSTEM_GUID 10
#define PEF_CFGPARM_ID_PEF_ALERT_STRING_TABLE_SIZE 11
#define PEF_CFGPARM_ID_PEF_ALERT_STRING_KEY 12
#define PEF_CFGPARM_ID_PEF_ALERT_STRING_TABLE_ENTRY 13
	uint8_t id;
	uint8_t set;
	uint8_t block;
} __attribute__ ((packed));

struct pef_cfgparm_set_in_progress {
#define PEF_SET_IN_PROGRESS_COMMIT_WRITE 0x02 
#define PEF_SET_IN_PROGRESS 0x01
	uint8_t data1;
} __attribute__ ((packed));

struct pef_cfgparm_control {
#define PEF_CONTROL_ENABLE_ALERT_STARTUP_DELAY 0x08
#define PEF_CONTROL_ENABLE_STARTUP_DELAY 0x04
#define PEF_CONTROL_ENABLE_EVENT_MESSAGES 0x02
#define PEF_CONTROL_ENABLE 0x01
	uint8_t data1;
} __attribute__ ((packed));

static struct bit_desc_map
pef_b2s_control __attribute__((unused)) = {
BIT_DESC_MAP_ALL,
{	{"PEF",							PEF_CONTROL_ENABLE},
	{"PEF event messages",		PEF_CONTROL_ENABLE_EVENT_MESSAGES},
	{"PEF startup delay",		PEF_CONTROL_ENABLE_STARTUP_DELAY},
	{"Alert startup delay",		PEF_CONTROL_ENABLE_ALERT_STARTUP_DELAY},
	{NULL}
}	};

struct pef_cfgparm_action {
#define PEF_ACTION_ENABLE_DIAGNOSTIC_INTERRUPT 0x20
#define PEF_ACTION_ENABLE_OEM 0x10
#define PEF_ACTION_ENABLE_POWER_CYCLE 0x08
#define PEF_ACTION_ENABLE_RESET 0x04
#define PEF_ACTION_ENABLE_POWER_DOWN 0x02
#define PEF_ACTION_ENABLE_ALERT 0x01
	uint8_t data1;
} __attribute__ ((packed));

struct pef_cfgparm_startup_delay {
	uint8_t data1;
} __attribute__ ((packed));

struct pef_cfgparm_alert_startup_delay {
	uint8_t data1;
} __attribute__ ((packed));

struct pef_cfgparm_filter_table_size {
#define PEF_FILTER_TABLE_SIZE_MASK 0x7f
	uint8_t data1;
} __attribute__ ((packed));

struct pef_cfgparm_filter_table_entry {
#define PEF_FILTER_TABLE_ID_MASK 0x7f
	uint8_t data1;
	struct pef_table_entry entry;
} __attribute__ ((packed));

struct pef_cfgparm_filter_table_data_1 {
	uint8_t data1;
	uint8_t data2;
} __attribute__ ((packed));

struct pef_cfgparm_policy_table_size {
#define PEF_POLICY_TABLE_SIZE_MASK 0x7f
	uint8_t data1;
} __attribute__ ((packed));

struct pef_cfgparm_policy_table_entry {
#define PEF_POLICY_TABLE_ID_MASK 0x7f
	uint8_t data1;
	struct pef_policy_entry entry;
} __attribute__ ((packed));

struct pef_cfgparm_system_guid {
#define PEF_SYSTEM_GUID_USED_IN_PET 0x01
	uint8_t data1;
	uint8_t guid[16];
} __attribute__ ((packed));

struct pef_cfgparm_alert_string_table_size {
#define PEF_ALERT_STRING_TABLE_SIZE_MASK 0x7f
	uint8_t data1;
} __attribute__ ((packed));

struct pef_cfgparm_alert_string_keys {
#define PEF_ALERT_STRING_ID_MASK 0x7f
	uint8_t data1;
#define PEF_EVENT_FILTER_ID_MASK 0x7f
	uint8_t data2;
#define PEF_ALERT_STRING_SET_ID_MASK 0x7f
	uint8_t data3;
} __attribute__ ((packed));

struct pef_cfgparm_alert_string_table_entry {
	uint8_t id;
	uint8_t blockno;
	uint8_t block[16];
} __attribute__ ((packed));

/* PEF - LAN */

struct pef_lan_cfgparm_selector {
#define PEF_LAN_CFGPARM_CH_REVISION_ONLY_MASK 0x80
#define PEF_LAN_CFGPARM_CH_MASK 0x0f
#define PEF_LAN_CFGPARM_ID_PET_COMMUNITY 16
#define PEF_LAN_CFGPARM_ID_DEST_COUNT 17
#define PEF_LAN_CFGPARM_ID_DESTTYPE 18
#define PEF_LAN_CFGPARM_ID_DESTADDR 19
	uint8_t ch;
	uint8_t id;
	uint8_t set;
	uint8_t block;
} __attribute__ ((packed));

struct pef_lan_cfgparm_dest_size {
#define PEF_LAN_DEST_TABLE_SIZE_MASK 0x0f
	uint8_t data1;
} __attribute__ ((packed));

struct pef_lan_cfgparm_dest_type {
#define PEF_LAN_DEST_TYPE_ID_MASK 0x0f
	uint8_t dest;
#define PEF_LAN_DEST_TYPE_ACK 0x80
#define PEF_LAN_DEST_TYPE_MASK 0x07
#define PEF_LAN_DEST_TYPE_PET 0
#define PEF_LAN_DEST_TYPE_OEM_1 6
#define PEF_LAN_DEST_TYPE_OEM_2 7
	uint8_t dest_type;
	uint8_t alert_timeout;
#define PEF_LAN_RETRIES_MASK 0x07
	uint8_t retries;
} __attribute__ ((packed));

static struct bit_desc_map
pef_b2s_lan_desttype __attribute__((unused)) = {
BIT_DESC_MAP_LIST,
{	{"Acknowledged",		PEF_LAN_DEST_TYPE_ACK},
	{"PET",					PEF_LAN_DEST_TYPE_PET},
	{"OEM 1",				PEF_LAN_DEST_TYPE_OEM_1},
	{"OEM 2",				PEF_LAN_DEST_TYPE_OEM_2},
	{NULL}
}	};

struct pef_lan_cfgparm_dest_info {
#define PEF_LAN_DEST_MASK 0x0f
	uint8_t dest;
#define PEF_LAN_DEST_ADDRTYPE_MASK 0xf0
#define PEF_LAN_DEST_ADDRTYPE_SHIFT 4
#define PEF_LAN_DEST_ADDRTYPE_IPV4_MAC 0x00
	uint8_t addr_type;
#define PEF_LAN_DEST_GATEWAY_USE_BACKUP 0x01
	uint8_t gateway;
	uint8_t ip[4];
	uint8_t mac[6];
} __attribute__ ((packed));

/* PEF - Serial/PPP */

struct pef_serial_cfgparm_selector {
#define PEF_SERIAL_CFGPARM_CH_REVISION_ONLY_MASK 0x80
#define PEF_SERIAL_CFGPARM_CH_MASK 0x0f
#define PEF_SERIAL_CFGPARM_ID_DEST_COUNT 16
#define PEF_SERIAL_CFGPARM_ID_DESTINFO 17
#define PEF_SERIAL_CFGPARM_ID_DEST_DIAL_STRING_COUNT 20
#define PEF_SERIAL_CFGPARM_ID_DEST_DIAL_STRING 21
#define PEF_SERIAL_CFGPARM_ID_TAP_ACCT_COUNT 24
#define PEF_SERIAL_CFGPARM_ID_TAP_ACCT_INFO 25
#define PEF_SERIAL_CFGPARM_ID_TAP_ACCT_PAGER_STRING 27
	uint8_t ch;
	uint8_t id;
	uint8_t set;
	uint8_t block;
} __attribute__ ((packed));

struct pef_serial_cfgparm_dest_size {
#define PEF_SERIAL_DEST_TABLE_SIZE_MASK 0x0f
	uint8_t data1;
} __attribute__ ((packed));

struct pef_serial_cfgparm_dest_info {
#define PEF_SERIAL_DEST_MASK 0x0f
	uint8_t dest;
#define PEF_SERIAL_DEST_TYPE_ACK 0x80
#define PEF_SERIAL_DEST_TYPE_MASK 0x0f
#define PEF_SERIAL_DEST_TYPE_DIAL 0
#define PEF_SERIAL_DEST_TYPE_TAP 1
#define PEF_SERIAL_DEST_TYPE_PPP 2
#define PEF_SERIAL_DEST_TYPE_BASIC_CALLBACK 3
#define PEF_SERIAL_DEST_TYPE_PPP_CALLBACK 4
#define PEF_SERIAL_DEST_TYPE_OEM_1 14
#define PEF_SERIAL_DEST_TYPE_OEM_2 15
	uint8_t dest_type;
	uint8_t alert_timeout;
#define PEF_SERIAL_RETRIES_MASK 0x77
#define PEF_SERIAL_RETRIES_POST_CONNECT_MASK 0x70
#define PEF_SERIAL_RETRIES_PRE_CONNECT_MASK 0x07
	uint8_t retries;
#define PEF_SERIAL_DIALPAGE_STRING_ID_MASK 0xf0
#define PEF_SERIAL_DIALPAGE_STRING_ID_SHIFT 4
#define PEF_SERIAL_TAP_PAGE_SERVICE_ID_MASK 0x0f 
#define PEF_SERIAL_PPP_ACCT_IPADDR_ID_MASK 0xf0
#define PEF_SERIAL_PPP_ACCT_IPADDR_ID_SHIFT 4
#define PEF_SERIAL_PPP_ACCT_ID_MASK 0x0f
#define PEF_SERIAL_CALLBACK_IPADDR_ID_MASK 0x0f
#define PEF_SERIAL_CALLBACK_IPADDR_ID_SHIFT 4
#define PEF_SERIAL_CALLBACK_ACCT_ID_MASK 0xf0
	uint8_t data5;
} __attribute__ ((packed));

static struct bit_desc_map
pef_b2s_serial_desttype __attribute__((unused)) = {
BIT_DESC_MAP_LIST,
{	{"Acknowledged",		PEF_SERIAL_DEST_TYPE_ACK},
	{"TAP page",			PEF_SERIAL_DEST_TYPE_TAP},
	{"PPP PET",				PEF_SERIAL_DEST_TYPE_PPP},
	{"Basic callback",	PEF_SERIAL_DEST_TYPE_BASIC_CALLBACK},
	{"PPP callback",		PEF_SERIAL_DEST_TYPE_PPP_CALLBACK},
	{"OEM 1",				PEF_SERIAL_DEST_TYPE_OEM_1},
	{"OEM 2",				PEF_SERIAL_DEST_TYPE_OEM_2},
	{NULL}
}	};

struct pef_serial_cfgparm_dial_string_count {
#define PEF_SERIAL_DIAL_STRING_COUNT_MASK 0x0f
	uint8_t data1;
} __attribute__ ((packed));

struct pef_serial_cfgparm_dial_string {
#define PEF_SERIAL_DIAL_STRING_MASK 0x0f
	uint8_t data1;
	uint8_t data2;
	uint8_t data3;
} __attribute__ ((packed));

struct pef_serial_cfgparm_tap_acct_count {
#define PEF_SERIAL_TAP_ACCT_COUNT_MASK 0x0f
	uint8_t data1;
} __attribute__ ((packed));

struct pef_serial_cfgparm_tap_acct_info {
	uint8_t data1;
#define PEF_SERIAL_TAP_ACCT_INFO_DIAL_STRING_ID_MASK 0xf0
#define PEF_SERIAL_TAP_ACCT_INFO_DIAL_STRING_ID_SHIFT 4
#define PEF_SERIAL_TAP_ACCT_INFO_SVC_SETTINGS_ID_MASK 0x0f
	uint8_t data2;
} __attribute__ ((packed));

struct pef_serial_cfgparm_tap_svc_settings {
	uint8_t data1;
#define PEF_SERIAL_TAP_CONFIRMATION_ACK_AFTER_ETX 0x0
#define PEF_SERIAL_TAP_CONFIRMATION_211_ACK_AFTER_ETX 0x01
#define PEF_SERIAL_TAP_CONFIRMATION_21X_ACK_AFTER_ETX 0x02
	uint8_t confirmation_flags;
	uint8_t service_type[3];
	uint8_t escape_mask[4];
	uint8_t timeout_parms[3];
	uint8_t retry_parms[2];
} __attribute__ ((packed));

static struct bit_desc_map
pef_b2s_tap_svc_confirm __attribute__((unused)) = {
BIT_DESC_MAP_LIST,
{	{"ACK",						PEF_SERIAL_TAP_CONFIRMATION_ACK_AFTER_ETX},
	{"211+ACK",					PEF_SERIAL_TAP_CONFIRMATION_211_ACK_AFTER_ETX},
	{"{211|213}+ACK",			PEF_SERIAL_TAP_CONFIRMATION_21X_ACK_AFTER_ETX},
	{NULL}
}	};

#if 0		/* FYI : config parm groupings */
	struct pef_config_parms {								/* PEF */
		struct pef_cfgparm_set_in_progress;
		struct pef_cfgparm_control;
		struct pef_cfgparm_action;
		struct pef_cfgparm_startup_delay;				/* in seconds, 1-based */
		struct pef_cfgparm_alert_startup_delay;		/* in seconds, 1-based */
		struct pef_cfgparm_filter_table_size;			/* 1-based, READ-ONLY */
		struct pef_cfgparm_filter_table_entry;
		struct pef_cfgparm_filter_table_data_1;
		struct pef_cfgparm_policy_table_size;
		struct pef_cfgparm_policy_table_entry;
		struct pef_cfgparm_system_guid;
		struct pef_cfgparm_alert_string_table_size;
		struct pef_cfgparm_alert_string_keys;
		struct pef_cfgparm_alert_string_table_entry;
	} __attribute__ ((packed));

	struct pef_lan_config_parms {							/* LAN */
		struct pef_lan_cfgparm_set_in_progress;
		struct pef_lan_cfgparm_auth_capabilities;
		struct pef_lan_cfgparm_auth_type;
		struct pef_lan_cfgparm_ip_address;
		struct pef_lan_cfgparm_ip_address_source;
		struct pef_lan_cfgparm_mac_address;
		struct pef_lan_cfgparm_subnet_mask;
		struct pef_lan_cfgparm_ipv4_header_parms;
		struct pef_lan_cfgparm_primary_rmcp_port;
		struct pef_lan_cfgparm_secondary_rmcp_port;
		struct pef_lan_cfgparm_bmc_generated_arp_control;
		struct pef_lan_cfgparm_gratuitous_arp;
		struct pef_lan_cfgparm_default_gateway_ipaddr;
		struct pef_lan_cfgparm_default_gateway_macaddr;
		struct pef_lan_cfgparm_backup_gateway_ipaddr;
		struct pef_lan_cfgparm_backup_gateway_macaddr;
		struct pef_lan_cfgparm_pet_community;
		struct pef_lan_cfgparm_destination_count;
		struct pef_lan_cfgparm_destination_type;
		struct pef_lan_cfgparm_destination_ipaddr;
	} __attribute__ ((packed));

	struct pef_serial_config_parms {						/* Serial/PPP */
		struct pef_serial_cfgparm_set_in_progress;
		struct pef_serial_cfgparm_auth_capabilities;
		struct pef_serial_cfgparm_auth_type;
		struct pef_serial_cfgparm_connection_mode;
		struct pef_serial_cfgparm_idle_timeout;
		struct pef_serial_cfgparm_callback_control;
		struct pef_serial_cfgparm_session_termination;
		struct pef_serial_cfgparm_ipmi_settings;
		struct pef_serial_cfgparm_mux_control;
		struct pef_serial_cfgparm_modem_ring_time;
		struct pef_serial_cfgparm_modem_init_string;
		struct pef_serial_cfgparm_modem_escape_sequence;
		struct pef_serial_cfgparm_modem_hangup_sequence;
		struct pef_serial_cfgparm_modem_dial_command;
		struct pef_serial_cfgparm_page_blackout_interval;
		struct pef_serial_cfgparm_pet_community;
		struct pef_serial_cfgparm_destination_count;
		struct pef_serial_cfgparm_destination_info;
		struct pef_serial_cfgparm_call_retry_interval;
		struct pef_serial_cfgparm_destination_settings;
		struct pef_serial_cfgparm_dialstring_count;
		struct pef_serial_cfgparm_dialstring_info;
		struct pef_serial_cfgparm_ipaddr_count;
		struct pef_serial_cfgparm_ipaddr_info;
		struct pef_serial_cfgparm_tap_acct_count;
		struct pef_serial_cfgparm_tap_acct_info;
		struct pef_serial_cfgparm_tap_acct_passwords;			/* WRITE only */
		struct pef_serial_cfgparm_tap_pager_id_strings;
		struct pef_serial_cfgparm_tap_service_settings;
		struct pef_serial_cfgparm_terminal_mode_config;
		struct pef_serial_cfgparm_ppp_otions;
		struct pef_serial_cfgparm_ppp_primary_rmcp_port;
		struct pef_serial_cfgparm_ppp_secondary_rmcp_port;
		struct pef_serial_cfgparm_ppp_link_auth;
		struct pef_serial_cfgparm_ppp_chap_name;
		struct pef_serial_cfgparm_ppp_accm;
		struct pef_serial_cfgparm_ppp_snoop_accm;
		struct pef_serial_cfgparm_ppp_acct_count;
		struct pef_serial_cfgparm_ppp_acct_dialstring_selector;
		struct pef_serial_cfgparm_ppp_acct_ipaddrs;
		struct pef_serial_cfgparm_ppp_acct_user_names;
		struct pef_serial_cfgparm_ppp_acct_user_domains;
		struct pef_serial_cfgparm_ppp_acct_user_passwords;		/* WRITE only */
		struct pef_serial_cfgparm_ppp_acct_auth_settings;
		struct pef_serial_cfgparm_ppp_acct_connect_hold_times;
		struct pef_serial_cfgparm_ppp_udp_proxy_ipheader;
		struct pef_serial_cfgparm_ppp_udp_proxy_xmit_bufsize;
		struct pef_serial_cfgparm_ppp_udp_proxy_recv_bufsize;
		struct pef_serial_cfgparm_ppp_remote_console_ipaddr;
	} __attribute__ ((packed));
#endif

#define IPMI_CMD_GET_PEF_CAPABILITIES 0x10
#define IPMI_CMD_GET_PEF_CONFIG_PARMS 0x13
#define IPMI_CMD_GET_LAST_PROCESSED_EVT_ID 0x15
#define IPMI_CMD_GET_SYSTEM_GUID 0x37
#define IPMI_CMD_GET_CHANNEL_INFO 0x42
#define IPMI_CMD_LAN_GET_CONFIG 0x02
#define IPMI_CMD_SERIAL_GET_CONFIG 0x11

const char * ipmi_pef_bit_desc(struct bit_desc_map * map, uint32_t val);
void ipmi_pef_print_flags(struct bit_desc_map * map, flg_e type, uint32_t val);
void ipmi_pef_print_dec(const char * text, uint32_t val);
void ipmi_pef_print_hex(const char * text, uint32_t val);
void ipmi_pef_print_1xd(const char * text, uint32_t val);
void ipmi_pef_print_2xd(const char * text, uint8_t u1, uint8_t u2);
void ipmi_pef_print_str(const char * text, const char * val);

int ipmi_pef_main(struct ipmi_intf * intf, int argc, char ** argv);

#endif /* IPMI_PEF_H */
