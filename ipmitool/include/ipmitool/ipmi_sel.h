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

#ifndef IPMI_SEL_H
#define IPMI_SEL_H

#include <inttypes.h>
#include <ipmitool/ipmi.h>

#define IPMI_CMD_GET_SEL_INFO		0x40
#define IPMI_CMD_GET_SEL_ALLOC_INFO	0x41
#define IPMI_CMD_RESERVE_SEL		0x42
#define IPMI_CMD_GET_SEL_ENTRY		0x43
#define IPMI_CMD_ADD_SEL_ENTRY		0x44
#define IPMI_CMD_PARTIAL_ADD_SEL_ENTRY	0x45
#define IPMI_CMD_DELETE_SEL_ENTRY	0x46
#define IPMI_CMD_CLEAR_SEL		0x47
#define IPMI_CMD_GET_SEL_TIME		0x48
#define IPMI_CMD_SET_SEL_TIME		0x49
#define IPMI_CMD_GET_AUX_LOG_STATUS	0x5A
#define IPMI_CMD_SET_AUX_LOG_STATUS	0x5B

enum {
	IPMI_EVENT_CLASS_DISCRETE,
	IPMI_EVENT_CLASS_DIGITAL,
	IPMI_EVENT_CLASS_THRESHOLD,
	IPMI_EVENT_CLASS_OEM,
};

struct sel_get_rq {
	unsigned short	reserve_id;
	unsigned short	record_id;
	unsigned char	offset;
	unsigned char	length;
} __attribute__ ((packed));

struct sel_event_record {
	unsigned short	record_id;
	unsigned char	record_type;
	uint32_t	timestamp;
	unsigned short	gen_id;
	unsigned char	evm_rev;
	unsigned char	sensor_type;
	unsigned char	sensor_num;
	unsigned char	event_type : 7;
	unsigned char	event_dir  : 1;
#define DATA_BYTE2_SPECIFIED_MASK 0xc0    /* event_data[0] bit mask */
#define DATA_BYTE3_SPECIFIED_MASK 0x30    /* event_data[0] bit mask */
#define EVENT_OFFSET_MASK         0x0f    /* event_data[0] bit mask */
	unsigned char	event_data[3];
} __attribute__ ((packed));

struct sel_oem_record_ts {
	unsigned short	next_id;
	unsigned short	record_id;
	unsigned char	record_type;
	uint32_t	timestamp;
	unsigned char	mfg_id[3];
	unsigned char	oem_defined[6];
} __attribute__ ((packed));

struct sel_oem_record_nots {
	unsigned short	next_id;
	unsigned short	record_id;
	unsigned char	record_type;
	unsigned char	oem_defined[13];
} __attribute__ ((packed));

struct ipmi_event_sensor_types {
	unsigned char	code;
	unsigned char	offset;
#define ALL_OFFSETS_SPECIFIED  0xff
        unsigned char   data;
	unsigned char	class;
	const char	* type;
	const char	* desc;
};

static struct ipmi_event_sensor_types generic_event_types[] __attribute__((unused)) = {
	/* Threshold Based States */
	{ 0x01, 0x00, 0xff, IPMI_EVENT_CLASS_THRESHOLD, "Threshold", "Lower Non-critical - going low" },
	{ 0x01, 0x01, 0xff, IPMI_EVENT_CLASS_THRESHOLD, "Threshold", "Lower Non-critical - going high" },
	{ 0x01, 0x02, 0xff, IPMI_EVENT_CLASS_THRESHOLD, "Threshold", "Lower Critical - going low" },
	{ 0x01, 0x03, 0xff, IPMI_EVENT_CLASS_THRESHOLD, "Threshold", "Lower Critical - going high" },
	{ 0x01, 0x04, 0xff, IPMI_EVENT_CLASS_THRESHOLD, "Threshold", "Lower Non-recoverable - going low" },
	{ 0x01, 0x05, 0xff, IPMI_EVENT_CLASS_THRESHOLD, "Threshold", "Lower Non-recoverable - going high" },
	{ 0x01, 0x06, 0xff, IPMI_EVENT_CLASS_THRESHOLD, "Threshold", "Upper Non-critical - going low" },
	{ 0x01, 0x07, 0xff, IPMI_EVENT_CLASS_THRESHOLD, "Threshold", "Upper Non-critical - going high" },
	{ 0x01, 0x08, 0xff, IPMI_EVENT_CLASS_THRESHOLD, "Threshold", "Upper Critical - going low" },
	{ 0x01, 0x09, 0xff, IPMI_EVENT_CLASS_THRESHOLD, "Threshold", "Upper Critical - going high" },
	{ 0x01, 0x0a, 0xff, IPMI_EVENT_CLASS_THRESHOLD, "Threshold", "Upper Non-recoverable - going low" },
	{ 0x01, 0x0b, 0xff, IPMI_EVENT_CLASS_THRESHOLD, "Threshold", "Upper Non-recoverable - going high" },
	/* DMI-based "usage state" States */
	{ 0x02, 0x00, 0xff, IPMI_EVENT_CLASS_DISCRETE, "Usage State", "Transition to Idle" },
	{ 0x02, 0x01, 0xff, IPMI_EVENT_CLASS_DISCRETE, "Usage State", "Transition to Active" },
	{ 0x02, 0x02, 0xff, IPMI_EVENT_CLASS_DISCRETE, "Usage State", "Transition to Busy" },
	/* Digital-Discrete Event States */
	{ 0x03, 0x00, 0xff, IPMI_EVENT_CLASS_DIGITAL, "Digital State",  "State Deasserted" },
	{ 0x03, 0x01, 0xff, IPMI_EVENT_CLASS_DIGITAL, "Digital State",  "State Asserted" },
	{ 0x04, 0x00, 0xff, IPMI_EVENT_CLASS_DIGITAL, "Digital State",  "Predictive Failure Deasserted" },
	{ 0x04, 0x01, 0xff, IPMI_EVENT_CLASS_DIGITAL, "Digital State",  "Predictive Failure Asserted" },
	{ 0x05, 0x00, 0xff, IPMI_EVENT_CLASS_DIGITAL, "Digital State",  "Limit Not Exceeded" },
	{ 0x05, 0x01, 0xff, IPMI_EVENT_CLASS_DIGITAL, "Digital State",  "Limit Exceeded" },
	{ 0x06, 0x00, 0xff, IPMI_EVENT_CLASS_DIGITAL, "Digital State",  "Performance Met" },
	{ 0x06, 0x01, 0xff, IPMI_EVENT_CLASS_DIGITAL, "Digital State",  "Performance Lags" },
	/* Severity Event States */
	{ 0x07, 0x00, 0xff, IPMI_EVENT_CLASS_DISCRETE, "Severity State", "Transition to OK" },
	{ 0x07, 0x01, 0xff, IPMI_EVENT_CLASS_DISCRETE, "Severity State", "Transition to Non-critial from OK" },
	{ 0x07, 0x02, 0xff, IPMI_EVENT_CLASS_DISCRETE, "Severity State", "Transition to Critical from less severe" },
	{ 0x07, 0x03, 0xff, IPMI_EVENT_CLASS_DISCRETE, "Severity State", "Transition to Non-recoverable from less severe" },
	{ 0x07, 0x04, 0xff, IPMI_EVENT_CLASS_DISCRETE, "Severity State", "Transition to Non-critical from more severe" },
	{ 0x07, 0x05, 0xff, IPMI_EVENT_CLASS_DISCRETE, "Severity State", "Transition to Critical from Non-recoverable" },
	{ 0x07, 0x06, 0xff, IPMI_EVENT_CLASS_DISCRETE, "Severity State", "Transition to Non-recoverable" },
	{ 0x07, 0x07, 0xff, IPMI_EVENT_CLASS_DISCRETE, "Severity State", "Monitor" },
	{ 0x07, 0x08, 0xff, IPMI_EVENT_CLASS_DISCRETE, "Severity State", "Informational" },
	/* Availability Status States */
	{ 0x08, 0x00, 0xff, IPMI_EVENT_CLASS_DIGITAL, "Availability State",  "Device Removed/Absent" },
	{ 0x08, 0x01, 0xff, IPMI_EVENT_CLASS_DIGITAL, "Availability State",  "Device Inserted/Present" },
	{ 0x09, 0x00, 0xff, IPMI_EVENT_CLASS_DIGITAL, "Availability State",  "Device Disabled" },
	{ 0x09, 0x01, 0xff, IPMI_EVENT_CLASS_DIGITAL, "Availability State",  "Device Enabled" },
	{ 0x0a, 0x00, 0xff, IPMI_EVENT_CLASS_DISCRETE, "Availability State", "Transition to Running" },
	{ 0x0a, 0x01, 0xff, IPMI_EVENT_CLASS_DISCRETE, "Availability State", "Transition to In Test" },
	{ 0x0a, 0x02, 0xff, IPMI_EVENT_CLASS_DISCRETE, "Availability State", "Transition to Power Off" },
	{ 0x0a, 0x03, 0xff, IPMI_EVENT_CLASS_DISCRETE, "Availability State", "Transition to On Line" },
	{ 0x0a, 0x04, 0xff, IPMI_EVENT_CLASS_DISCRETE, "Availability State", "Transition to Off Line" },
	{ 0x0a, 0x05, 0xff, IPMI_EVENT_CLASS_DISCRETE, "Availability State", "Transition to Off Duty" },
	{ 0x0a, 0x06, 0xff, IPMI_EVENT_CLASS_DISCRETE, "Availability State", "Transition to Degraded" },
	{ 0x0a, 0x07, 0xff, IPMI_EVENT_CLASS_DISCRETE, "Availability State", "Transition to Power Save" },
	{ 0x0a, 0x08, 0xff, IPMI_EVENT_CLASS_DISCRETE, "Availability State", "Install Error" },
	/* Redundancy States */
	{ 0x0b, 0x00, 0xff, IPMI_EVENT_CLASS_DISCRETE, "Redundancy State", "Fully Redundant" },
	{ 0x0b, 0x01, 0xff, IPMI_EVENT_CLASS_DISCRETE, "Redundancy State", "Redundancy Lost" },
	{ 0x0b, 0x02, 0xff, IPMI_EVENT_CLASS_DISCRETE, "Redundancy State", "Redundancy Degraded" },
	{ 0x0b, 0x03, 0xff, IPMI_EVENT_CLASS_DISCRETE, "Redundancy State", "Non-Redundant: Sufficient from Redundant" },
	{ 0x0b, 0x04, 0xff, IPMI_EVENT_CLASS_DISCRETE, "Redundancy State", "Non-Redundant: Sufficient from Insufficient" },
	{ 0x0b, 0x05, 0xff, IPMI_EVENT_CLASS_DISCRETE, "Redundancy State", "Non-Redundant: Insufficient Resources" },
	{ 0x0b, 0x06, 0xff, IPMI_EVENT_CLASS_DISCRETE, "Redundancy State", "Redundancy Degraded from Fully Redundant" },
	{ 0x0b, 0x07, 0xff, IPMI_EVENT_CLASS_DISCRETE, "Redundancy State", "Redundancy Degraded from Non-Redundant" },
	/* ACPI Device Power States */
	{ 0x0c, 0x00, 0xff, IPMI_EVENT_CLASS_DISCRETE, "ACPI Device Power State", "D0 Power State" },
	{ 0x0c, 0x01, 0xff, IPMI_EVENT_CLASS_DISCRETE, "ACPI Device Power State", "D1 Power State" },
	{ 0x0c, 0x02, 0xff, IPMI_EVENT_CLASS_DISCRETE, "ACPI Device Power State", "D2 Power State" },
	{ 0x0c, 0x03, 0xff, IPMI_EVENT_CLASS_DISCRETE, "ACPI Device Power State", "D3 Power State" },
	/* END */
	{ 0x00, 0x00, 0xff, 0x00, NULL, NULL },
};

static struct ipmi_event_sensor_types sensor_specific_types[] __attribute__((unused)) = {
	{ 0x00, 0x00, 0xff, IPMI_EVENT_CLASS_DISCRETE, "Reserved",	NULL },
	{ 0x01, 0x00, 0xff, IPMI_EVENT_CLASS_DISCRETE, "Temperature",	NULL },
	{ 0x02, 0x00, 0xff, IPMI_EVENT_CLASS_DISCRETE, "Voltage",	NULL },
	{ 0x03, 0x00, 0xff, IPMI_EVENT_CLASS_DISCRETE, "Current",	NULL },
	{ 0x04, 0x00, 0xff, IPMI_EVENT_CLASS_DISCRETE, "Fan",		NULL },

	{ 0x05, 0x00, 0xff, IPMI_EVENT_CLASS_DISCRETE, "Physical Security", "General Chassis intrusion" },
	{ 0x05, 0x01, 0xff, IPMI_EVENT_CLASS_DISCRETE, "Physical Security", "Drive Bay intrusion" },
	{ 0x05, 0x02, 0xff, IPMI_EVENT_CLASS_DISCRETE, "Physical Security", "I/O Card area intrusion" },
	{ 0x05, 0x03, 0xff, IPMI_EVENT_CLASS_DISCRETE, "Physical Security", "Processor area intrusion" },
	{ 0x05, 0x04, 0xff, IPMI_EVENT_CLASS_DISCRETE, "Physical Security", "System unplugged from LAN" },
	{ 0x05, 0x05, 0xff, IPMI_EVENT_CLASS_DISCRETE, "Physical Security", "Unauthorized dock/undock" },
	{ 0x05, 0x06, 0xff, IPMI_EVENT_CLASS_DISCRETE, "Physical Security", "FAN area intrusion" },

	{ 0x06, 0x00, 0xff, IPMI_EVENT_CLASS_DISCRETE, "Platform Security", "Front Panel Lockout violation attempted" },
	{ 0x06, 0x01, 0xff, IPMI_EVENT_CLASS_DISCRETE, "Platform Security", "Pre-boot password viiolation - user password" },
	{ 0x06, 0x02, 0xff, IPMI_EVENT_CLASS_DISCRETE, "Platform Security", "Pre-boot password violation - setup password" },
	{ 0x06, 0x03, 0xff, IPMI_EVENT_CLASS_DISCRETE, "Platform Security", "Pre-boot password violation - network boot password" },
	{ 0x06, 0x04, 0xff, IPMI_EVENT_CLASS_DISCRETE, "Platform Security", "Other pre-boot password violation" },
	{ 0x06, 0x05, 0xff, IPMI_EVENT_CLASS_DISCRETE, "Platform Security", "Out-of-band access password violation" },

	{ 0x07, 0x00, 0xff, IPMI_EVENT_CLASS_DISCRETE, "Processor", "IERR" },
	{ 0x07, 0x01, 0xff, IPMI_EVENT_CLASS_DISCRETE, "Processor", "Thermal Trip" },
	{ 0x07, 0x02, 0xff, IPMI_EVENT_CLASS_DISCRETE, "Processor", "FRB1/BIST failure" },
	{ 0x07, 0x03, 0xff, IPMI_EVENT_CLASS_DISCRETE, "Processor", "FRB2/Hang in POST failure" },
	{ 0x07, 0x04, 0xff, IPMI_EVENT_CLASS_DISCRETE, "Processor", "FRB3/Processor startup/init failure" },
	{ 0x07, 0x05, 0xff, IPMI_EVENT_CLASS_DISCRETE, "Processor", "Configuration Error" },
	{ 0x07, 0x06, 0xff, IPMI_EVENT_CLASS_DISCRETE, "Processor", "SM BIOS Uncorrectable CPU-complex Error" },
	{ 0x07, 0x07, 0xff, IPMI_EVENT_CLASS_DISCRETE, "Processor", "Presence detected" },
	{ 0x07, 0x08, 0xff, IPMI_EVENT_CLASS_DISCRETE, "Processor", "Disabled" },
	{ 0x07, 0x09, 0xff, IPMI_EVENT_CLASS_DISCRETE, "Processor", "Terminator presence detected" },

	{ 0x08, 0x00, 0xff, IPMI_EVENT_CLASS_DISCRETE, "Power Supply", "Presence detected" },
	{ 0x08, 0x01, 0xff, IPMI_EVENT_CLASS_DISCRETE, "Power Supply", "Failure detected" },
	{ 0x08, 0x02, 0xff, IPMI_EVENT_CLASS_DISCRETE, "Power Supply", "Predictive failure" },
	{ 0x08, 0x03, 0xff, IPMI_EVENT_CLASS_DISCRETE, "Power Supply", "Power Supply AC lost" },
	{ 0x08, 0x04, 0xff, IPMI_EVENT_CLASS_DISCRETE, "Power Supply", "AC lost or out-of-range" },
	{ 0x08, 0x05, 0xff, IPMI_EVENT_CLASS_DISCRETE, "Power Supply", "AC out-of-range, but present" },

	{ 0x09, 0x00, 0xff, IPMI_EVENT_CLASS_DISCRETE, "Power Unit", "Power off/down" },
	{ 0x09, 0x01, 0xff, IPMI_EVENT_CLASS_DISCRETE, "Power Unit", "Power cycle" },
	{ 0x09, 0x02, 0xff, IPMI_EVENT_CLASS_DISCRETE, "Power Unit", "240VA power down" },
	{ 0x09, 0x03, 0xff, IPMI_EVENT_CLASS_DISCRETE, "Power Unit", "Interlock power down" },
	{ 0x09, 0x04, 0xff, IPMI_EVENT_CLASS_DISCRETE, "Power Unit", "AC lost" },
	{ 0x09, 0x05, 0xff, IPMI_EVENT_CLASS_DISCRETE, "Power Unit", "Soft-power control failure" },
	{ 0x09, 0x06, 0xff, IPMI_EVENT_CLASS_DISCRETE, "Power Unit", "Failure detected" },
	{ 0x09, 0x07, 0xff, IPMI_EVENT_CLASS_DISCRETE, "Power Unit", "Predictive failure" },

	{ 0x0a, 0x00, 0xff, IPMI_EVENT_CLASS_DISCRETE, "Cooling Device", NULL },
	{ 0x0b, 0x00, 0xff, IPMI_EVENT_CLASS_DISCRETE, "Other Units-based Sensor", NULL },

	{ 0x0c, 0x00, 0xff, IPMI_EVENT_CLASS_DISCRETE, "Memory", "Correctable ECC" },
	{ 0x0c, 0x01, 0xff, IPMI_EVENT_CLASS_DISCRETE, "Memory", "Uncorrectable ECC" },
	{ 0x0c, 0x02, 0xff, IPMI_EVENT_CLASS_DISCRETE, "Memory", "Parity" },
	{ 0x0c, 0x03, 0xff, IPMI_EVENT_CLASS_DISCRETE, "Memory", "Memory Scrub Failed" },
	{ 0x0c, 0x04, 0xff, IPMI_EVENT_CLASS_DISCRETE, "Memory", "Memory Device Disabled" },
	{ 0x0c, 0x05, 0xff, IPMI_EVENT_CLASS_DISCRETE, "Memory", "Correctable ECC logging limit reached" },

	{ 0x0d, 0x00, 0xff, IPMI_EVENT_CLASS_DISCRETE, "Drive Slot", NULL },
	{ 0x0e, 0x00, 0xff, IPMI_EVENT_CLASS_DISCRETE, "POST Memory Resize", NULL },

	{ 0x0f, 0x00, 0xff, IPMI_EVENT_CLASS_DISCRETE, "System Firmware", "Error" },
	{ 0x0f, 0x01, 0xff, IPMI_EVENT_CLASS_DISCRETE, "System Firmware", "Hang" },
	{ 0x0f, 0x02, 0x00, IPMI_EVENT_CLASS_DISCRETE, "System Firmware", "Progress 1" },
	{ 0x0f, 0x02, 0x01, IPMI_EVENT_CLASS_DISCRETE, "System Firmware", "Progress 2" },
	{ 0x0f, 0x02, 0x02, IPMI_EVENT_CLASS_DISCRETE, "System Firmware", "Progress 3" },
	{ 0x0f, 0x02, 0x03, IPMI_EVENT_CLASS_DISCRETE, "System Firmware", "Progress 4" },
	{ 0x0f, 0x02, 0x04, IPMI_EVENT_CLASS_DISCRETE, "System Firmware", "Progress 5" },
	{ 0x0f, 0x02, 0x13, IPMI_EVENT_CLASS_DISCRETE, "System Firmware", "System Boot Initiated" },

	{ 0x10, 0x00, 0xff, IPMI_EVENT_CLASS_DISCRETE, "Event Logging Disabled", "Correctable memory error logging disabled" },
	{ 0x10, 0x01, 0xff, IPMI_EVENT_CLASS_DISCRETE, "Event Logging Disabled", "Event logging disabled" },
	{ 0x10, 0x02, 0xff, IPMI_EVENT_CLASS_DISCRETE, "Event Logging Disabled", "Log area reset/cleared" },
	{ 0x10, 0x03, 0xff, IPMI_EVENT_CLASS_DISCRETE, "Event Logging Disabled", "All event logging disabled" },
	{ 0x10, 0x04, 0xff, IPMI_EVENT_CLASS_DISCRETE, "Event Logging Disabled", "Log full" },

	{ 0x11, 0x00, 0xff, IPMI_EVENT_CLASS_DISCRETE, "Watchdog 1", "BIOS Reset" },
	{ 0x11, 0x01, 0xff, IPMI_EVENT_CLASS_DISCRETE, "Watchdog 1", "OS Reset" },
	{ 0x11, 0x02, 0xff, IPMI_EVENT_CLASS_DISCRETE, "Watchdog 1", "OS Shut Down" },
	{ 0x11, 0x03, 0xff, IPMI_EVENT_CLASS_DISCRETE, "Watchdog 1", "OS Power Down" },
	{ 0x11, 0x04, 0xff, IPMI_EVENT_CLASS_DISCRETE, "Watchdog 1", "OS Power Cycle" },
	{ 0x11, 0x05, 0xff, IPMI_EVENT_CLASS_DISCRETE, "Watchdog 1", "OS NMI/diag Interrupt" },
	{ 0x11, 0x06, 0xff, IPMI_EVENT_CLASS_DISCRETE, "Watchdog 1", "OS Expired" },
	{ 0x11, 0x07, 0xff, IPMI_EVENT_CLASS_DISCRETE, "Watchdog 1", "OS pre-timeout Interrupt" },

	{ 0x12, 0x00, 0xff, IPMI_EVENT_CLASS_DISCRETE, "System Event", "System Reconfigured" },
	{ 0x12, 0x01, 0xff, IPMI_EVENT_CLASS_DISCRETE, "System Event", "OEM System boot event" },
	{ 0x12, 0x02, 0xff, IPMI_EVENT_CLASS_DISCRETE, "System Event", "Undetermined system hardware failure" },
	{ 0x12, 0x03, 0xff, IPMI_EVENT_CLASS_DISCRETE, "System Event", "Entry added to auxillary log" },
	{ 0x12, 0x04, 0xff, IPMI_EVENT_CLASS_DISCRETE, "System Event", "PEF Action" },
	{ 0x12, 0x05, 0xff, IPMI_EVENT_CLASS_DISCRETE, "System Event", "Timestamp Clock Sync." },

	{ 0x13, 0x00, 0xff, IPMI_EVENT_CLASS_DISCRETE, "Critical Interrupt", "Front Panel NMI" },
	{ 0x13, 0x01, 0xff, IPMI_EVENT_CLASS_DISCRETE, "Critical Interrupt", "Bus Timeout" },
	{ 0x13, 0x02, 0xff, IPMI_EVENT_CLASS_DISCRETE, "Critical Interrupt", "I/O Channel check NMI" },
	{ 0x13, 0x03, 0xff, IPMI_EVENT_CLASS_DISCRETE, "Critical Interrupt", "Software NMI" },
	{ 0x13, 0x04, 0xff, IPMI_EVENT_CLASS_DISCRETE, "Critical Interrupt", "PCI PERR" },
	{ 0x13, 0x05, 0xff, IPMI_EVENT_CLASS_DISCRETE, "Critical Interrupt", "PCI SERR" },
	{ 0x13, 0x06, 0xff, IPMI_EVENT_CLASS_DISCRETE, "Critical Interrupt", "EISA failsafe timeout" },
	{ 0x13, 0x07, 0xff, IPMI_EVENT_CLASS_DISCRETE, "Critical Interrupt", "Bus Correctable error" },
	{ 0x13, 0x08, 0xff, IPMI_EVENT_CLASS_DISCRETE, "Critical Interrupt", "Bus Uncorrectable error" },
	{ 0x13, 0x09, 0xff, IPMI_EVENT_CLASS_DISCRETE, "Critical Interrupt", "Fatal NMI" },

	{ 0x14, 0x00, 0xff, IPMI_EVENT_CLASS_DISCRETE, "Button", "Power Button pressed" },
	{ 0x14, 0x01, 0xff, IPMI_EVENT_CLASS_DISCRETE, "Button", "Sleep Button pressed" },
	{ 0x14, 0x02, 0xff, IPMI_EVENT_CLASS_DISCRETE, "Button", "Reset Button pressed" },

	{ 0x15, 0x00, 0xff, IPMI_EVENT_CLASS_DISCRETE, "Module/Board", NULL },
	{ 0x16, 0x00, 0xff, IPMI_EVENT_CLASS_DISCRETE, "Microcontroller/Coprocessor", NULL },
	{ 0x17, 0x00, 0xff, IPMI_EVENT_CLASS_DISCRETE, "Add-in Card", NULL },
	{ 0x18, 0x00, 0xff, IPMI_EVENT_CLASS_DISCRETE, "Chassis", NULL },
	{ 0x19, 0x00, 0xff, IPMI_EVENT_CLASS_DISCRETE, "Chip Set", NULL },
	{ 0x1a, 0x00, 0xff, IPMI_EVENT_CLASS_DISCRETE, "Other FRU", NULL },
	{ 0x1b, 0x00, 0xff, IPMI_EVENT_CLASS_DISCRETE, "Cable/Interconnect", NULL },
	{ 0x1c, 0x00, 0xff, IPMI_EVENT_CLASS_DISCRETE, "Terminator", NULL },

	{ 0x1d, 0x00, 0xff, IPMI_EVENT_CLASS_DISCRETE, "System Boot Initiated", "Initiated by power up" },
	{ 0x1d, 0x01, 0xff, IPMI_EVENT_CLASS_DISCRETE, "System Boot Initiated", "Initiated by hard reset" },
	{ 0x1d, 0x02, 0xff, IPMI_EVENT_CLASS_DISCRETE, "System Boot Initiated", "Initiated by warm reset" },
	{ 0x1d, 0x03, 0xff, IPMI_EVENT_CLASS_DISCRETE, "System Boot Initiated", "User requested PXE boot" },
	{ 0x1d, 0x04, 0xff, IPMI_EVENT_CLASS_DISCRETE, "System Boot Initiated", "Automatic boot to diagnostic" },

	{ 0x1e, 0x00, 0xff, IPMI_EVENT_CLASS_DISCRETE, "Boot Error", "No bootable media" },
	{ 0x1e, 0x01, 0xff, IPMI_EVENT_CLASS_DISCRETE, "Boot Error", "Non-bootable disk in drive" },
	{ 0x1e, 0x02, 0xff, IPMI_EVENT_CLASS_DISCRETE, "Boot Error", "PXE server not found" },
	{ 0x1e, 0x03, 0xff, IPMI_EVENT_CLASS_DISCRETE, "Boot Error", "Invalid boot sector" },
	{ 0x1e, 0x04, 0xff, IPMI_EVENT_CLASS_DISCRETE, "Boot Error", "Timeout waiting for selection" },

	{ 0x1f, 0x00, 0xff, IPMI_EVENT_CLASS_DISCRETE, "OS Boot", "A: boot completed" },
	{ 0x1f, 0x01, 0xff, IPMI_EVENT_CLASS_DISCRETE, "OS Boot", "C: boot completed" },
	{ 0x1f, 0x02, 0xff, IPMI_EVENT_CLASS_DISCRETE, "OS Boot", "PXE boot completed" },
	{ 0x1f, 0x03, 0xff, IPMI_EVENT_CLASS_DISCRETE, "OS Boot", "Diagnostic boot completed" },
	{ 0x1f, 0x04, 0xff, IPMI_EVENT_CLASS_DISCRETE, "OS Boot", "CD-ROM boot completed" },
	{ 0x1f, 0x05, 0xff, IPMI_EVENT_CLASS_DISCRETE, "OS Boot", "ROM boot completed" },
	{ 0x1f, 0x06, 0xff, IPMI_EVENT_CLASS_DISCRETE, "OS Boot", "boot completed - device not specified" },

	{ 0x20, 0x00, 0xff, IPMI_EVENT_CLASS_DISCRETE, "OS Critical Stop", "Stop during OS load/init" },
	{ 0x20, 0x01, 0xff, IPMI_EVENT_CLASS_DISCRETE, "OS Critical Stop", "Run-time stop" },

	{ 0x21, 0x00, 0xff, IPMI_EVENT_CLASS_DISCRETE, "Slot/Connector", "Fault Status asserted" },
	{ 0x21, 0x01, 0xff, IPMI_EVENT_CLASS_DISCRETE, "Slot/Connector", "Identify Status asserted" },
	{ 0x21, 0x02, 0xff, IPMI_EVENT_CLASS_DISCRETE, "Slot/Connector", "Slot/Connector Device installed/attached" },
	{ 0x21, 0x03, 0xff, IPMI_EVENT_CLASS_DISCRETE, "Slot/Connector", "Slot/Connector ready for device installation" },
	{ 0x21, 0x04, 0xff, IPMI_EVENT_CLASS_DISCRETE, "Slot/Connector", "Slot/Connector ready for device removal" },
	{ 0x21, 0x05, 0xff, IPMI_EVENT_CLASS_DISCRETE, "Slot/Connector", "Slot Power is off" },
	{ 0x21, 0x06, 0xff, IPMI_EVENT_CLASS_DISCRETE, "Slot/Connector", "Slot/Connector device removal request" },
	{ 0x21, 0x07, 0xff, IPMI_EVENT_CLASS_DISCRETE, "Slot/Connector", "Interlock asserted" },
	{ 0x21, 0x08, 0xff, IPMI_EVENT_CLASS_DISCRETE, "Slot/Connector", "Slot is disabled" },

	{ 0x22, 0x00, 0xff, IPMI_EVENT_CLASS_DISCRETE, "System ACPI Power State", "S0/G0: working" },
	{ 0x22, 0x01, 0xff, IPMI_EVENT_CLASS_DISCRETE, "System ACPI Power State", "S1: sleeping with system hw & processor context maintained" },
	{ 0x22, 0x02, 0xff, IPMI_EVENT_CLASS_DISCRETE, "System ACPI Power State", "S2: sleeping, processor context lost" },
	{ 0x22, 0x03, 0xff, IPMI_EVENT_CLASS_DISCRETE, "System ACPI Power State", "S3: sleeping, processor & hw context lost, memory retained" },
	{ 0x22, 0x04, 0xff, IPMI_EVENT_CLASS_DISCRETE, "System ACPI Power State", "S4: non-volatile sleep/suspend-to-disk" },
	{ 0x22, 0x05, 0xff, IPMI_EVENT_CLASS_DISCRETE, "System ACPI Power State", "S5/G2: soft-off" },
	{ 0x22, 0x06, 0xff, IPMI_EVENT_CLASS_DISCRETE, "System ACPI Power State", "S4/S5: soft-off" },
	{ 0x22, 0x07, 0xff, IPMI_EVENT_CLASS_DISCRETE, "System ACPI Power State", "G3: mechanical off" },
	{ 0x22, 0x08, 0xff, IPMI_EVENT_CLASS_DISCRETE, "System ACPI Power State", "Sleeping in S1/S2/S3 state" },
	{ 0x22, 0x09, 0xff, IPMI_EVENT_CLASS_DISCRETE, "System ACPI Power State", "G1: sleeping" },
	{ 0x22, 0x0a, 0xff, IPMI_EVENT_CLASS_DISCRETE, "System ACPI Power State", "S5: entered by override" },
	{ 0x22, 0x0b, 0xff, IPMI_EVENT_CLASS_DISCRETE, "System ACPI Power State", "Legacy ON state" },
	{ 0x22, 0x0c, 0xff, IPMI_EVENT_CLASS_DISCRETE, "System ACPI Power State", "Legacy OFF state" },
	{ 0x22, 0x0e, 0xff, IPMI_EVENT_CLASS_DISCRETE, "System ACPI Power State", "Unknown" },

	{ 0x23, 0x00, 0xff, IPMI_EVENT_CLASS_DISCRETE, "Watchdog 2", "Timer expired" },
	{ 0x23, 0x01, 0xff, IPMI_EVENT_CLASS_DISCRETE, "Watchdog 2", "Hard reset" },
	{ 0x23, 0x02, 0xff, IPMI_EVENT_CLASS_DISCRETE, "Watchdog 2", "Power down" },
	{ 0x23, 0x03, 0xff, IPMI_EVENT_CLASS_DISCRETE, "Watchdog 2", "Power cycle" },
	{ 0x23, 0x04, 0xff, IPMI_EVENT_CLASS_DISCRETE, "Watchdog 2", "reserved" },
	{ 0x23, 0x05, 0xff, IPMI_EVENT_CLASS_DISCRETE, "Watchdog 2", "reserved" },
	{ 0x23, 0x06, 0xff, IPMI_EVENT_CLASS_DISCRETE, "Watchdog 2", "reserved" },
	{ 0x23, 0x07, 0xff, IPMI_EVENT_CLASS_DISCRETE, "Watchdog 2", "reserved" },
	{ 0x23, 0x08, 0xff, IPMI_EVENT_CLASS_DISCRETE, "Watchdog 2", "Timer interrupt" },

	{ 0x24, 0x00, 0xff, IPMI_EVENT_CLASS_DISCRETE, "Platform Alert", "Platform generated page" },
	{ 0x24, 0x01, 0xff, IPMI_EVENT_CLASS_DISCRETE, "Platform Alert", "Platform generated LAN alert" },
	{ 0x24, 0x02, 0xff, IPMI_EVENT_CLASS_DISCRETE, "Platform Alert", "Platform Event Trap generated" },
	{ 0x24, 0x03, 0xff, IPMI_EVENT_CLASS_DISCRETE, "Platform Alert", "Platform generated SNMP trap, OEM format" },

	{ 0x25, 0x00, 0xff, IPMI_EVENT_CLASS_DISCRETE, "Entity Presence", "Present" },
	{ 0x25, 0x01, 0xff, IPMI_EVENT_CLASS_DISCRETE, "Entity Presence", "Absent" },
	{ 0x25, 0x02, 0xff, IPMI_EVENT_CLASS_DISCRETE, "Entity Presence", "Disabled" },

	{ 0x26, 0x00, 0xff, IPMI_EVENT_CLASS_DISCRETE, "Monitor ASIC/IC", NULL },

	{ 0x27, 0x00, 0xff, IPMI_EVENT_CLASS_DISCRETE, "LAN", "Heartbeat Lost" },
	{ 0x27, 0x01, 0xff, IPMI_EVENT_CLASS_DISCRETE, "LAN", "Heartbeat" },

	{ 0x28, 0x00, 0xff, IPMI_EVENT_CLASS_DISCRETE, "Management Subsystem Health", "Sensor access degraded or unavailable" },
	{ 0x28, 0x01, 0xff, IPMI_EVENT_CLASS_DISCRETE, "Management Subsystem Health", "Controller access degraded or unavailable" },
	{ 0x28, 0x02, 0xff, IPMI_EVENT_CLASS_DISCRETE, "Management Subsystem Health", "Management controller off-line" },
	{ 0x28, 0x03, 0xff, IPMI_EVENT_CLASS_DISCRETE, "Management Subsystem Health", "Management controller unavailable" },

	{ 0x29, 0x00, 0xff, IPMI_EVENT_CLASS_DISCRETE, "Battery", "Low" },
	{ 0x29, 0x01, 0xff, IPMI_EVENT_CLASS_DISCRETE, "Battery", "Failed" },
	{ 0x29, 0x02, 0xff, IPMI_EVENT_CLASS_DISCRETE, "Battery", "Presence Detected" },

	{ 0x00, 0x00, 0x00, 0x00, NULL, NULL },
};

int ipmi_sel_main(struct ipmi_intf *, int, char **);
void ipmi_sel_print_std_entry(struct sel_event_record * evt);
void ipmi_sel_print_std_entry_verbose(struct sel_event_record * evt);
void ipmi_get_event_desc(struct sel_event_record * rec, char ** desc);
const char * ipmi_sel_get_sensor_type(unsigned char code);

#endif /* IPMI_SEL_H */
