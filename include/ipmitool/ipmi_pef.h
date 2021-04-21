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

#pragma once

#include <ipmitool/ipmi.h>

/* PEF */

struct pef_capabilities {		/* "get pef capabilities" response */
	uint8_t version;
	uint8_t actions;						/* mapped by PEF_ACTION_xxx */
	uint8_t event_filter_count;
};

typedef enum {
	P_TRUE,
	P_SUPP,
	P_ACTV,
	P_ABLE,
} flg_e;

#ifdef HAVE_PRAGMA_PACK
#pragma pack(1)
#endif
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
} ATTRIBUTE_PACKING;
#ifdef HAVE_PRAGMA_PACK
#pragma pack(0)
#endif

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


#ifdef HAVE_PRAGMA_PACK
#pragma pack(1)
#endif
struct pef_policy_entry {
#define PEF_POLICY_ID_MASK 0xf0
#define PEF_POLICY_ID_SHIFT 4
#define PEF_POLICY_DISABLED 0xF7
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
} ATTRIBUTE_PACKING;
#ifdef HAVE_PRAGMA_PACK
#pragma pack(0)
#endif


#ifdef HAVE_PRAGMA_PACK
#pragma pack(1)
#endif
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
} ATTRIBUTE_PACKING;
#ifdef HAVE_PRAGMA_PACK
#pragma pack(0)
#endif

#ifdef HAVE_PRAGMA_PACK
#pragma pack(1)
#endif
struct pef_cfgparm_set_in_progress {
#define PEF_SET_IN_PROGRESS_COMMIT_WRITE 0x02 
#define PEF_SET_IN_PROGRESS 0x01
	uint8_t data1;
} ATTRIBUTE_PACKING;
#ifdef HAVE_PRAGMA_PACK
#pragma pack(0)
#endif

#ifdef HAVE_PRAGMA_PACK
#pragma pack(1)
#endif
struct pef_cfgparm_control {
#define PEF_CONTROL_ENABLE_ALERT_STARTUP_DELAY 0x08
#define PEF_CONTROL_ENABLE_STARTUP_DELAY 0x04
#define PEF_CONTROL_ENABLE_EVENT_MESSAGES 0x02
#define PEF_CONTROL_ENABLE 0x01
	uint8_t data1;
} ATTRIBUTE_PACKING;
#ifdef HAVE_PRAGMA_PACK
#pragma pack(0)
#endif


#ifdef HAVE_PRAGMA_PACK
#pragma pack(1)
#endif
struct pef_cfgparm_action {
#define PEF_ACTION_ENABLE_DIAGNOSTIC_INTERRUPT 0x20
#define PEF_ACTION_ENABLE_OEM 0x10
#define PEF_ACTION_ENABLE_POWER_CYCLE 0x08
#define PEF_ACTION_ENABLE_RESET 0x04
#define PEF_ACTION_ENABLE_POWER_DOWN 0x02
#define PEF_ACTION_ENABLE_ALERT 0x01
	uint8_t data1;
} ATTRIBUTE_PACKING;
#ifdef HAVE_PRAGMA_PACK
#pragma pack(0)
#endif

#ifdef HAVE_PRAGMA_PACK
#pragma pack(1)
#endif
struct pef_cfgparm_startup_delay {
	uint8_t data1;
} ATTRIBUTE_PACKING;
#ifdef HAVE_PRAGMA_PACK
#pragma pack(0)
#endif

#ifdef HAVE_PRAGMA_PACK
#pragma pack(1)
#endif
struct pef_cfgparm_alert_startup_delay {
	uint8_t data1;
} ATTRIBUTE_PACKING;
#ifdef HAVE_PRAGMA_PACK
#pragma pack(0)
#endif

#ifdef HAVE_PRAGMA_PACK
#pragma pack(1)
#endif
struct pef_cfgparm_filter_table_size {
#define PEF_FILTER_TABLE_SIZE_MASK 0x7f
	uint8_t data1;
} ATTRIBUTE_PACKING;
#ifdef HAVE_PRAGMA_PACK
#pragma pack(0)
#endif

#ifdef HAVE_PRAGMA_PACK
#pragma pack(1)
#endif
struct pef_cfgparm_filter_table_entry {
# define PEF_FILTER_DISABLED 0x7F
# define PEF_FILTER_ENABLED 0x80
# define PEF_FILTER_TABLE_ID_MASK 0x7F
	uint8_t data1;
	struct pef_table_entry entry;
} ATTRIBUTE_PACKING;
#ifdef HAVE_PRAGMA_PACK
#pragma pack(0)
#endif

#ifdef HAVE_PRAGMA_PACK
#pragma pack(1)
#endif
struct pef_cfgparm_filter_table_data_1 {
	uint8_t id;
	uint8_t cfg;
} ATTRIBUTE_PACKING;
#ifdef HAVE_PRAGMA_PACK
#pragma pack(0)
#endif

#ifdef HAVE_PRAGMA_PACK
#pragma pack(1)
#endif
struct pef_cfgparm_policy_table_size {
#define PEF_POLICY_TABLE_SIZE_MASK 0x7f
	uint8_t data1;
} ATTRIBUTE_PACKING;
#ifdef HAVE_PRAGMA_PACK
#pragma pack(0)
#endif

#ifdef HAVE_PRAGMA_PACK
#pragma pack(1)
#endif
struct pef_cfgparm_policy_table_entry {
#define PEF_POLICY_TABLE_ID_MASK 0x7f
	uint8_t data1;
	struct pef_policy_entry entry;
} ATTRIBUTE_PACKING;
#ifdef HAVE_PRAGMA_PACK
#pragma pack(0)
#endif

#ifdef HAVE_PRAGMA_PACK
#pragma pack(1)
#endif
struct pef_cfgparm_system_guid {
#define PEF_SYSTEM_GUID_USED_IN_PET 0x01
	uint8_t data1;
	uint8_t guid[16];
} ATTRIBUTE_PACKING;
#ifdef HAVE_PRAGMA_PACK
#pragma pack(0)
#endif

#ifdef HAVE_PRAGMA_PACK
#pragma pack(1)
#endif
struct pef_cfgparm_alert_string_table_size {
#define PEF_ALERT_STRING_TABLE_SIZE_MASK 0x7f
	uint8_t data1;
} ATTRIBUTE_PACKING;
#ifdef HAVE_PRAGMA_PACK
#pragma pack(0)
#endif

#ifdef HAVE_PRAGMA_PACK
#pragma pack(1)
#endif
struct pef_cfgparm_alert_string_keys {
#define PEF_ALERT_STRING_ID_MASK 0x7f
	uint8_t data1;
#define PEF_EVENT_FILTER_ID_MASK 0x7f
	uint8_t data2;
#define PEF_ALERT_STRING_SET_ID_MASK 0x7f
	uint8_t data3;
} ATTRIBUTE_PACKING;
#ifdef HAVE_PRAGMA_PACK
#pragma pack(0)
#endif

#ifdef HAVE_PRAGMA_PACK
#pragma pack(1)
#endif
struct pef_cfgparm_alert_string_table_entry {
	uint8_t id;
	uint8_t blockno;
	uint8_t block[16];
} ATTRIBUTE_PACKING;
#ifdef HAVE_PRAGMA_PACK
#pragma pack(0)
#endif

/* PEF - LAN */
#ifdef HAVE_PRAGMA_PACK
#pragma pack(1)
#endif
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
} ATTRIBUTE_PACKING;
#ifdef HAVE_PRAGMA_PACK
#pragma pack(0)
#endif

#ifdef HAVE_PRAGMA_PACK
#pragma pack(1)
#endif
struct pef_lan_cfgparm_dest_size {
#define PEF_LAN_DEST_TABLE_SIZE_MASK 0x0f
	uint8_t data1;
} ATTRIBUTE_PACKING;
#ifdef HAVE_PRAGMA_PACK
#pragma pack(0)
#endif

#ifdef HAVE_PRAGMA_PACK
#pragma pack(1)
#endif
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
} ATTRIBUTE_PACKING;
#ifdef HAVE_PRAGMA_PACK
#pragma pack(0)
#endif


#ifdef HAVE_PRAGMA_PACK
#pragma pack(1)
#endif
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
} ATTRIBUTE_PACKING;
#ifdef HAVE_PRAGMA_PACK
#pragma pack(0)
#endif

/* PEF - Serial/PPP */
#ifdef HAVE_PRAGMA_PACK
#pragma pack(1)
#endif
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
} ATTRIBUTE_PACKING;
#ifdef HAVE_PRAGMA_PACK
#pragma pack(0)
#endif

#ifdef HAVE_PRAGMA_PACK
#pragma pack(1)
#endif
struct pef_serial_cfgparm_dest_size {
#define PEF_SERIAL_DEST_TABLE_SIZE_MASK 0x0f
	uint8_t data1;
} ATTRIBUTE_PACKING;
#ifdef HAVE_PRAGMA_PACK
#pragma pack(0)
#endif

#ifdef HAVE_PRAGMA_PACK
#pragma pack(1)
#endif
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
} ATTRIBUTE_PACKING;
#ifdef HAVE_PRAGMA_PACK
#pragma pack(0)
#endif


#ifdef HAVE_PRAGMA_PACK
#pragma pack(1)
#endif
struct pef_serial_cfgparm_dial_string_count {
#define PEF_SERIAL_DIAL_STRING_COUNT_MASK 0x0f
	uint8_t data1;
} ATTRIBUTE_PACKING;
#ifdef HAVE_PRAGMA_PACK
#pragma pack(0)
#endif

#ifdef HAVE_PRAGMA_PACK
#pragma pack(1)
#endif
struct pef_serial_cfgparm_dial_string {
#define PEF_SERIAL_DIAL_STRING_MASK 0x0f
	uint8_t data1;
	uint8_t data2;
	uint8_t data3;
} ATTRIBUTE_PACKING;
#ifdef HAVE_PRAGMA_PACK
#pragma pack(0)
#endif

#ifdef HAVE_PRAGMA_PACK
#pragma pack(1)
#endif
struct pef_serial_cfgparm_tap_acct_count {
#define PEF_SERIAL_TAP_ACCT_COUNT_MASK 0x0f
	uint8_t data1;
} ATTRIBUTE_PACKING;
#ifdef HAVE_PRAGMA_PACK
#pragma pack(0)
#endif

#ifdef HAVE_PRAGMA_PACK
#pragma pack(1)
#endif
struct pef_serial_cfgparm_tap_acct_info {
	uint8_t data1;
#define PEF_SERIAL_TAP_ACCT_INFO_DIAL_STRING_ID_MASK 0xf0
#define PEF_SERIAL_TAP_ACCT_INFO_DIAL_STRING_ID_SHIFT 4
#define PEF_SERIAL_TAP_ACCT_INFO_SVC_SETTINGS_ID_MASK 0x0f
	uint8_t data2;
} ATTRIBUTE_PACKING;
#ifdef HAVE_PRAGMA_PACK
#pragma pack(0)
#endif

#ifdef HAVE_PRAGMA_PACK
#pragma pack(1)
#endif
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
} ATTRIBUTE_PACKING;
#ifdef HAVE_PRAGMA_PACK
#pragma pack(0)
#endif


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
	} ATTRIBUTE_PACKING;

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
	} ATTRIBUTE_PACKING;

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
	} ATTRIBUTE_PACKING;
#endif

#define IPMI_CMD_GET_PEF_CAPABILITIES 0x10
#define IPMI_CMD_SET_PEF_CONFIG_PARMS 0x12
#define IPMI_CMD_GET_PEF_CONFIG_PARMS 0x13
#define IPMI_CMD_GET_LAST_PROCESSED_EVT_ID 0x15
#define IPMI_CMD_GET_SYSTEM_GUID 0x37
#define IPMI_CMD_GET_CHANNEL_INFO 0x42
#define IPMI_CMD_LAN_GET_CONFIG 0x02
#define IPMI_CMD_SERIAL_GET_CONFIG 0x11

struct pef_cfgparm_set_policy_table_entry
{
	uint8_t param_selector;
	uint8_t policy_id;
	struct pef_policy_entry entry;
} ATTRIBUTE_PACKING;

const char * ipmi_pef_bit_desc(struct bit_desc_map * map, uint32_t val);
void ipmi_pef_print_flags(struct bit_desc_map * map, flg_e type, uint32_t val);
void ipmi_pef_print_dec(const char * text, uint32_t val);
void ipmi_pef_print_hex(const char * text, uint32_t val);
void ipmi_pef_print_1xd(const char * text, uint32_t val);
void ipmi_pef_print_2xd(const char * text, uint8_t u1, uint8_t u2);
void ipmi_pef_print_str(const char * text, const char * val);

int ipmi_pef_main(struct ipmi_intf * intf, int argc, char ** argv);
