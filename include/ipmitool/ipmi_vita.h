/*
 * Copyright (c) Pigeon Point Systems. All right reserved
 */

#pragma once

/* VITA 46.11 commands */
#define VITA_GET_VSO_CAPABILITIES_CMD		0x00
#define VITA_FRU_CONTROL_CMD			0x04
#define VITA_GET_FRU_LED_PROPERTIES_CMD		0x05
#define VITA_GET_LED_COLOR_CAPABILITIES_CMD	0x06
#define VITA_SET_FRU_LED_STATE_CMD		0x07
#define VITA_GET_FRU_LED_STATE_CMD		0x08
#define VITA_SET_FRU_STATE_POLICY_BITS_CMD	0x0A
#define VITA_GET_FRU_STATE_POLICY_BITS_CMD	0x0B
#define VITA_SET_FRU_ACTIVATION_CMD		0x0C
#define VITA_GET_FRU_ADDRESS_INFO_CMD		0x40

/* VITA 46.11 site types */
#define VITA_FRONT_VPX_MODULE		0x00
#define VITA_POWER_ENTRY		0x01
#define VITA_CHASSIS_FRU		0x02
#define VITA_DEDICATED_CHMC		0x03
#define VITA_FAN_TRAY			0x04
#define VITA_FAN_TRAY_FILTER		0x05
#define VITA_ALARM_PANEL		0x06
#define VITA_XMC			0x07
#define VITA_VPX_RTM			0x09
#define VITA_FRONT_VME_MODULE		0x0A
#define VITA_FRONT_VXS_MODULE		0x0B
#define VITA_POWER_SUPPLY		0x0C
#define VITA_FRONT_VITA62_MODULE	0x0D
#define VITA_71_MODULE			0x0E
#define VITA_FMC			0x0F


#define GROUP_EXT_VITA		0x03

extern uint8_t
vita_discover(struct ipmi_intf *intf);

extern uint8_t
ipmi_vita_ipmb_address(struct ipmi_intf *intf);

extern int
ipmi_vita_main(struct ipmi_intf * intf, int argc, char ** argv);
