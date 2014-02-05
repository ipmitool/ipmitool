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

#ifndef IPMI_LANP_H
#define IPMI_LANP_H

#include <ipmitool/ipmi.h>

#define IPMI_LAN_SET_CONFIG	0x01
#define IPMI_LAN_GET_CONFIG	0x02
#define IPMI_LAN_SUSPEND_ARP	0x03
# define IPMI_LAN_SUSPEND_ARP_RESP (2)
# define IPMI_LAN_SUSPEND_ARP_GRAT (1)
#define IPMI_LAN_GET_STAT	0x04

#define IPMI_CHANNEL_NUMBER_MAX	0xe

#define IPMI_LANP_TIMEOUT		3
#define IPMI_LANP_RETRIES		10
#define IPMI_LANP_WRITE_UNLOCK		0
#define IPMI_LANP_WRITE_LOCK		1
#define IPMI_LANP_WRITE_COMMIT		2

enum {
	IPMI_LANP_SET_IN_PROGRESS,
	IPMI_LANP_AUTH_TYPE,
	IPMI_LANP_AUTH_TYPE_ENABLE,
	IPMI_LANP_IP_ADDR,
	IPMI_LANP_IP_ADDR_SRC,
	IPMI_LANP_MAC_ADDR,            /* 5 */
	IPMI_LANP_SUBNET_MASK,
	IPMI_LANP_IP_HEADER,
	IPMI_LANP_PRI_RMCP_PORT,
	IPMI_LANP_SEC_RMCP_PORT,
	IPMI_LANP_BMC_ARP,             /* 10 */
	IPMI_LANP_GRAT_ARP,
	IPMI_LANP_DEF_GATEWAY_IP,
	IPMI_LANP_DEF_GATEWAY_MAC,
	IPMI_LANP_BAK_GATEWAY_IP,
	IPMI_LANP_BAK_GATEWAY_MAC,     /* 15 */
	IPMI_LANP_SNMP_STRING,
	IPMI_LANP_NUM_DEST,
	IPMI_LANP_DEST_TYPE,
	IPMI_LANP_DEST_ADDR,
	IPMI_LANP_VLAN_ID,             /* 20 */
	IPMI_LANP_VLAN_PRIORITY,
	IPMI_LANP_RMCP_CIPHER_SUPPORT,
	IPMI_LANP_RMCP_CIPHERS,
	IPMI_LANP_RMCP_PRIV_LEVELS,
	IPMI_LANP_OEM_ALERT_STRING=96,
	IPMI_LANP_ALERT_RETRY=97,
	IPMI_LANP_UTC_OFFSET=98,
	IPMI_LANP_DHCP_SERVER_IP=192,
	IPMI_LANP_DHCP_SERVER_MAC=193,
	IPMI_LANP_DHCP_ENABLE=194,
	IPMI_LANP_CHAN_ACCESS_MODE=201,
};

static struct lan_param {
	int cmd;
	int size;
	char desc[24];
	uint8_t * data;
	int data_len;
} ipmi_lan_params[] __attribute__((unused)) = {
	{ IPMI_LANP_SET_IN_PROGRESS,	1,	"Set in Progress"	},
	{ IPMI_LANP_AUTH_TYPE,		1,	"Auth Type Support"	},
	{ IPMI_LANP_AUTH_TYPE_ENABLE,	5,	"Auth Type Enable"	},
	{ IPMI_LANP_IP_ADDR,		4,	"IP Address"		},
	{ IPMI_LANP_IP_ADDR_SRC,	1,	"IP Address Source"	},
	{ IPMI_LANP_MAC_ADDR,		6,	"MAC Address"		}, /* 5 */
	{ IPMI_LANP_SUBNET_MASK,	4,	"Subnet Mask"		},
	{ IPMI_LANP_IP_HEADER,		3,	"IP Header"		},
	{ IPMI_LANP_PRI_RMCP_PORT,	2,	"Primary RMCP Port"	},
	{ IPMI_LANP_SEC_RMCP_PORT,	2,	"Secondary RMCP Port"	},
	{ IPMI_LANP_BMC_ARP,		1,	"BMC ARP Control"	}, /* 10 */
	{ IPMI_LANP_GRAT_ARP,		1,	"Gratituous ARP Intrvl"	},
	{ IPMI_LANP_DEF_GATEWAY_IP,	4,	"Default Gateway IP"	},
	{ IPMI_LANP_DEF_GATEWAY_MAC,	6,	"Default Gateway MAC"	},
	{ IPMI_LANP_BAK_GATEWAY_IP,	4,	"Backup Gateway IP"	},
	{ IPMI_LANP_BAK_GATEWAY_MAC,	6,	"Backup Gateway MAC"	}, /* 15 */
	{ IPMI_LANP_SNMP_STRING,	18,	"SNMP Community String"	},
	{ IPMI_LANP_NUM_DEST,		1,	"Number of Destinations"},
	{ IPMI_LANP_DEST_TYPE,		4,	"Destination Type"	},
	{ IPMI_LANP_DEST_ADDR,		13,	"Destination Addresses"	},
	{ IPMI_LANP_VLAN_ID,		2,	"802.1q VLAN ID"        }, /* 20 */
	{ IPMI_LANP_VLAN_PRIORITY,	1,	"802.1q VLAN Priority"  },
	{ IPMI_LANP_RMCP_CIPHER_SUPPORT,1,	"RMCP+ Cipher Suite Count" },
	{ IPMI_LANP_RMCP_CIPHERS,	16,	"RMCP+ Cipher Suites"   },
	{ IPMI_LANP_RMCP_PRIV_LEVELS,	9,	"Cipher Suite Priv Max"	},
	{ IPMI_LANP_OEM_ALERT_STRING,	28,	"OEM Alert String"	}, /* 25 */
	{ IPMI_LANP_ALERT_RETRY,	1,	"Alert Retry Algorithm" },
	{ IPMI_LANP_UTC_OFFSET,		3,	"UTC Offset"		},
	{ IPMI_LANP_DHCP_SERVER_IP,	4,	"DHCP Server IP"	},
	{ IPMI_LANP_DHCP_SERVER_MAC,	6,	"DHDP Server MAC"	}, 
	{ IPMI_LANP_DHCP_ENABLE,	1,	"DHCP Enable"		}, /* 30 */
	{ IPMI_LANP_CHAN_ACCESS_MODE,	2,	"Channel Access Mode"	},
	{ -1 }
};

int  ipmi_lanp_main(struct ipmi_intf *, int, char **);

#endif /*IPMI_LANP_H*/
