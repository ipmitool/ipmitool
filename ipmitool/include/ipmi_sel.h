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

#ifndef _IPMI_SEL_H
#define _IPMI_SEL_H

#include <ipmi.h>

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
	unsigned short	next_id;
	unsigned short	record_id;
	unsigned char	record_type;
	unsigned long	timestamp;
	unsigned short	gen_id;
	unsigned char	evm_rev;
	unsigned char	sensor_type;
	unsigned char	sensor_num;
	unsigned char	event_type : 7;
	unsigned char	event_dir  : 1;
	unsigned char	event_data[3];
} __attribute__ ((packed));

struct sel_oem_record_ts {
	unsigned short	next_id;
	unsigned short	record_id;
	unsigned char	record_type;
	unsigned long	timestamp;
	unsigned char	mfg_id[3];
	unsigned char	oem_defined[6];
} __attribute__ ((packed));

struct sel_oem_record_nots {
	unsigned short	next_id;
	unsigned short	record_id;
	unsigned char	record_type;
	unsigned char	oem_defined[13];
} __attribute__ ((packed));


struct ipmi_event_type {
	unsigned char code;
	unsigned char offset;
	unsigned char class;
	const char * desc;
};

static struct ipmi_event_type event_types[] __attribute__((unused)) = {
	/* Threshold Based States */
	{ 0x01, 0x00, IPMI_EVENT_CLASS_THRESHOLD, "Lower Non-critical - going low" },
	{ 0x01, 0x01, IPMI_EVENT_CLASS_THRESHOLD, "Lower Non-critical - going high" },
	{ 0x01, 0x02, IPMI_EVENT_CLASS_THRESHOLD, "Lower Critical - going low" },
	{ 0x01, 0x03, IPMI_EVENT_CLASS_THRESHOLD, "Lower Critical - going high" },
	{ 0x01, 0x04, IPMI_EVENT_CLASS_THRESHOLD, "Lower Non-recoverable - going low" },
	{ 0x01, 0x05, IPMI_EVENT_CLASS_THRESHOLD, "Lower Non-recoverable - going high" },
	{ 0x01, 0x06, IPMI_EVENT_CLASS_THRESHOLD, "Upper Non-critical - going low" },
	{ 0x01, 0x07, IPMI_EVENT_CLASS_THRESHOLD, "Upper Non-critical - going high" },
	{ 0x01, 0x08, IPMI_EVENT_CLASS_THRESHOLD, "Upper Critical - going low" },
	{ 0x01, 0x09, IPMI_EVENT_CLASS_THRESHOLD, "Upper Critical - going high" },
	{ 0x01, 0x0a, IPMI_EVENT_CLASS_THRESHOLD, "Upper Non-recoverable - going low" },
	{ 0x01, 0x0b, IPMI_EVENT_CLASS_THRESHOLD, "Upper Non-recoverable - going high" },
	/* DMI-based "usage state" States */
	{ 0x02, 0x00, IPMI_EVENT_CLASS_DISCRETE,  "Transition to Idle" },
	{ 0x02, 0x01, IPMI_EVENT_CLASS_DISCRETE,  "Transition to Active" },
	{ 0x02, 0x02, IPMI_EVENT_CLASS_DISCRETE,  "Transition to Busy" },
	/* Digital-Discrete Event States */
	{ 0x03, 0x00, IPMI_EVENT_CLASS_DIGITAL,   "State Deasserted" },
	{ 0x03, 0x01, IPMI_EVENT_CLASS_DIGITAL,   "State Asserted" },
	{ 0x04, 0x00, IPMI_EVENT_CLASS_DIGITAL,   "Predictive Failure Deasserted" },
	{ 0x04, 0x01, IPMI_EVENT_CLASS_DIGITAL,   "Predictive Failure Asserted" },
	{ 0x05, 0x00, IPMI_EVENT_CLASS_DIGITAL,   "Limit Not Exceeded" },
	{ 0x05, 0x01, IPMI_EVENT_CLASS_DIGITAL,   "Limit Exceeded" },
	{ 0x06, 0x00, IPMI_EVENT_CLASS_DIGITAL,   "Performance Met" },
	{ 0x06, 0x01, IPMI_EVENT_CLASS_DIGITAL,   "Performance Lags" },
	/* Severity Event States */
	{ 0x07, 0x00, IPMI_EVENT_CLASS_DISCRETE,  "Transition to OK" },
	{ 0x07, 0x01, IPMI_EVENT_CLASS_DISCRETE,  "Transition to Non-critial from OK" },
	{ 0x07, 0x02, IPMI_EVENT_CLASS_DISCRETE,  "Transition to Critical from less severe" },
	{ 0x07, 0x03, IPMI_EVENT_CLASS_DISCRETE,  "Transition to Non-recoverable from less severe" },
	{ 0x07, 0x04, IPMI_EVENT_CLASS_DISCRETE,  "Transition to Non-critical from more severe" },
	{ 0x07, 0x05, IPMI_EVENT_CLASS_DISCRETE,  "Transition to Critical from Non-recoverable" },
	{ 0x07, 0x06, IPMI_EVENT_CLASS_DISCRETE,  "Transition to Non-recoverable" },
	{ 0x07, 0x07, IPMI_EVENT_CLASS_DISCRETE,  "Monitor" },
	{ 0x07, 0x08, IPMI_EVENT_CLASS_DISCRETE,  "Informational" },
	/* Availability Status States */
	{ 0x08, 0x00, IPMI_EVENT_CLASS_DIGITAL,   "Device Removed/Absent" },
	{ 0x08, 0x01, IPMI_EVENT_CLASS_DIGITAL,   "Device Inserted/Present" },
	{ 0x09, 0x00, IPMI_EVENT_CLASS_DIGITAL,   "Device Disabled" },
	{ 0x09, 0x01, IPMI_EVENT_CLASS_DIGITAL,   "Device Enabled" },
	{ 0x0a, 0x00, IPMI_EVENT_CLASS_DISCRETE,  "Transition to Running" },
	{ 0x0a, 0x01, IPMI_EVENT_CLASS_DISCRETE,  "Transition to In Test" },
	{ 0x0a, 0x02, IPMI_EVENT_CLASS_DISCRETE,  "Transition to Power Off" },
	{ 0x0a, 0x03, IPMI_EVENT_CLASS_DISCRETE,  "Transition to On Line" },
	{ 0x0a, 0x04, IPMI_EVENT_CLASS_DISCRETE,  "Transition to Off Line" },
	{ 0x0a, 0x05, IPMI_EVENT_CLASS_DISCRETE,  "Transition to Off Duty" },
	{ 0x0a, 0x06, IPMI_EVENT_CLASS_DISCRETE,  "Transition to Degraded" },
	{ 0x0a, 0x07, IPMI_EVENT_CLASS_DISCRETE,  "Transition to Power Save" },
	{ 0x0a, 0x08, IPMI_EVENT_CLASS_DISCRETE,  "Install Error" },
	/* Redundancy States */
	{ 0x0b, 0x00, IPMI_EVENT_CLASS_DISCRETE,  "Fully Redundant" },
	{ 0x0b, 0x01, IPMI_EVENT_CLASS_DISCRETE,  "Redundancy Lost" },
	{ 0x0b, 0x02, IPMI_EVENT_CLASS_DISCRETE,  "Redundancy Degraded" },
	{ 0x0b, 0x03, IPMI_EVENT_CLASS_DISCRETE,  "Non-Redundant: Sufficient from Redundant" },
	{ 0x0b, 0x04, IPMI_EVENT_CLASS_DISCRETE,  "Non-Redundant: Sufficient from Insufficient" },
	{ 0x0b, 0x05, IPMI_EVENT_CLASS_DISCRETE,  "Non-Redundant: Insufficient Resources" },
	{ 0x0b, 0x06, IPMI_EVENT_CLASS_DISCRETE,  "Redundancy Degraded from Fully Redundant" },
	{ 0x0b, 0x07, IPMI_EVENT_CLASS_DISCRETE,  "Redundancy Degraded from Non-Redundant" },
	/* ACPI Device Power States */
	{ 0x0c, 0x00, IPMI_EVENT_CLASS_DISCRETE,  "D0 Power State" },
	{ 0x0c, 0x01, IPMI_EVENT_CLASS_DISCRETE,  "D1 Power State" },
	{ 0x0c, 0x02, IPMI_EVENT_CLASS_DISCRETE,  "D2 Power State" },
	{ 0x0c, 0x03, IPMI_EVENT_CLASS_DISCRETE,  "D3 Power State" },
	/* END */
	{ 0x00, 0x00, 0x00, NULL },
};

struct ipmi_sensor_types {
	unsigned char code;
	unsigned char offset;
	const char * type;
	const char * event;
};

static struct ipmi_sensor_types sensor_types[] __attribute__((unused)) = {
	{ 0x00, 0x00, "Reserved",	NULL },
	{ 0x01, 0x00, "Temperature",	NULL },
	{ 0x02, 0x00, "Voltage",	NULL },
	{ 0x03, 0x00, "Current",	NULL },
	{ 0x04, 0x00, "Fan",		NULL },

	{ 0x05, 0x00, "Chassis Intrusion", "General Chassis intrusion" },
	{ 0x05, 0x01, "Chassis Intrusion", "Drive Bay intrusion" },
	{ 0x05, 0x02, "Chassis Intrusion", "I/O Card area intrusion" },
	{ 0x05, 0x03, "Chassis Intrusion", "Processor area intrusion" },
	{ 0x05, 0x04, "Chassis Intrusion", "System unplugged from LAN" },
	{ 0x05, 0x05, "Chassis Intrusion", "Unauthorized dock/undock" },
	{ 0x05, 0x06, "Chassis Intrusion", "FAN area intrusion" },

	{ 0x06, 0x00, "Platform Security", "Front Panel Lockout violation attempted" },
	{ 0x06, 0x01, "Platform Security", "Pre-boot password viiolation - user password" },
	{ 0x06, 0x02, "Platform Security", "Pre-boot password violation - setup password" },
	{ 0x06, 0x03, "Platform Security", "Pre-boot password violation - network boot password" },
	{ 0x06, 0x04, "Platform Security", "Other pre-boot password violation" },
	{ 0x06, 0x05, "Platform Security", "Out-of-band access password violation" },

	{ 0x07, 0x00, "Processor", "IERR" },
	{ 0x07, 0x01, "Processor", "Thermal Trip" },
	{ 0x07, 0x02, "Processor", "FRB1/BIST failure" },
	{ 0x07, 0x03, "Processor", "FRB2/Hang in POST failure" },
	{ 0x07, 0x04, "Processor", "FRB3/Processor startup/init failure" },
	{ 0x07, 0x05, "Processor", "Configuration Error" },
	{ 0x07, 0x06, "Processor", "SM BIOS Uncorrectable CPU-complex Error" },
	{ 0x07, 0x07, "Processor", "Presence detected" },
	{ 0x07, 0x08, "Processor", "Disabled" },
	{ 0x07, 0x09, "Processor", "Terminator presence detected" },

	{ 0x08, 0x00, "Power Supply", "Presence detected" },
	{ 0x08, 0x01, "Power Supply", "Failure detected" },
	{ 0x08, 0x02, "Power Supply", "Predictive failure" },
	{ 0x08, 0x03, "Power Supply", "Power Supply AC lost" },
	{ 0x08, 0x04, "Power Supply", "AC lost or out-of-range" },
	{ 0x08, 0x05, "Power Supply", "AC out-of-range, but present" },

	{ 0x09, 0x00, "Power Unit", "Power off/down" },
	{ 0x09, 0x01, "Power Unit", "Power cycle" },
	{ 0x09, 0x02, "Power Unit", "240VA power down" },
	{ 0x09, 0x03, "Power Unit", "Interlock power down" },
	{ 0x09, 0x04, "Power Unit", "AC lost" },
	{ 0x09, 0x05, "Power Unit", "Soft-power control failure" },
	{ 0x09, 0x06, "Power Unit", "Failure detected" },
	{ 0x09, 0x07, "Power Unit", "Predictive failure" },

	{ 0x0a, 0x00, "Cooling Device", NULL },
	{ 0x0b, 0x00, "Other Units-based Sensor", NULL },

	{ 0x0c, 0x00, "Memory", "Correctable ECC" },
	{ 0x0c, 0x01, "Memory", "Uncorrectable ECC" },
	{ 0x0c, 0x02, "Memory", "Parity" },
	{ 0x0c, 0x03, "Memory", "Memory Scrub Failed" },
	{ 0x0c, 0x04, "Memory", "Memory Device Disabled" },
	{ 0x0c, 0x05, "Memory", "Correctable ECC logging limit reached" },

	{ 0x0d, 0x00, "Drive Slot", NULL },
	{ 0x0e, 0x00, "POST Memory Resize", NULL },

	{ 0x0f, 0x00, "System Firmware", "Error" },
	{ 0x0f, 0x01, "System Firmware", "Hang" },
	{ 0x0f, 0x02, "System Firmware", "Progress" },

	{ 0x10, 0x00, "Event Logging Disabled", "Correctable memory error logging disabled" },
	{ 0x10, 0x01, "Event Logging Disabled", "Event logging disabled" },
	{ 0x10, 0x02, "Event Logging Disabled", "Log area reset/cleared" },
	{ 0x10, 0x03, "Event Logging Disabled", "All event logging disabled" },

	{ 0x11, 0x00, "Watchdog 1", "BIOS Reset" },
	{ 0x11, 0x01, "Watchdog 1", "OS Reset" },
	{ 0x11, 0x02, "Watchdog 1", "OS Shut Down" },
	{ 0x11, 0x03, "Watchdog 1", "OS Power Down" },
	{ 0x11, 0x04, "Watchdog 1", "OS Power Cycle" },
	{ 0x11, 0x05, "Watchdog 1", "OS NMI/diag Interrupt" },
	{ 0x11, 0x06, "Watchdog 1", "OS Expired" },
	{ 0x11, 0x07, "Watchdog 1", "OS pre-timeout Interrupt" },

	{ 0x12, 0x00, "System Event", "System Reconfigured" },
	{ 0x12, 0x01, "System Event", "OEM System boot event" },
	{ 0x12, 0x02, "System Event", "Undetermined system hardware failure" },
	{ 0x12, 0x03, "System Event", "Entry added to auxillary log" },
	{ 0x12, 0x04, "System Event", "PEF Action" },

	{ 0x13, 0x00, "Critical Interrupt", "Front Panel NMI" },
	{ 0x13, 0x01, "Critical Interrupt", "Bus Timeout" },
	{ 0x13, 0x02, "Critical Interrupt", "I/O Channel check NMI" },
	{ 0x13, 0x03, "Critical Interrupt", "Software NMI" },
	{ 0x13, 0x04, "Critical Interrupt", "PCI PERR" },
	{ 0x13, 0x05, "Critical Interrupt", "PCI SERR" },
	{ 0x13, 0x06, "Critical Interrupt", "EISA failsafe timeout" },
	{ 0x13, 0x07, "Critical Interrupt", "Bus Correctable error" },
	{ 0x13, 0x08, "Critical Interrupt", "Bus Uncorrectable error" },
	{ 0x13, 0x09, "Critical Interrupt", "Fatal NMI" },

	{ 0x14, 0x00, "Button", "Power Button pressed" },
	{ 0x14, 0x01, "Button", "Sleep Button pressed" },
	{ 0x14, 0x02, "Button", "Reset Button pressed" },

	{ 0x15, 0x00, "Module/Board", NULL },
	{ 0x16, 0x00, "Microcontroller/Coprocessor", NULL },
	{ 0x17, 0x00, "Add-in Card", NULL },
	{ 0x18, 0x00, "Chassis", NULL },
	{ 0x19, 0x00, "Chip Set", NULL },
	{ 0x1a, 0x00, "Other FRU", NULL },
	{ 0x1b, 0x00, "Cable/Interconnect", NULL },
	{ 0x1c, 0x00, "Terminator", NULL },

	{ 0x1d, 0x00, "System Boot Initiated", "Initiated by power up" },
	{ 0x1d, 0x01, "System Boot Initiated", "Initiated by hard reset" },
	{ 0x1d, 0x02, "System Boot Initiated", "Initiated by warm reset" },
	{ 0x1d, 0x03, "System Boot Initiated", "User requested PXE boot" },
	{ 0x1d, 0x04, "System Boot Initiated", "Automatic boot to diagnostic" },

	{ 0x1e, 0x00, "Boot Error", "No bootable media" },
	{ 0x1e, 0x01, "Boot Error", "Non-bootable disk in drive" },
	{ 0x1e, 0x02, "Boot Error", "PXE server not found" },
	{ 0x1e, 0x03, "Boot Error", "Invalid boot sector" },
	{ 0x1e, 0x04, "Boot Error", "Timeout waiting for selection" },

	{ 0x1f, 0x00, "OS Boot", "A: boot completed" },
	{ 0x1f, 0x01, "OS Boot", "C: boot completed" },
	{ 0x1f, 0x02, "OS Boot", "PXE boot completed" },
	{ 0x1f, 0x03, "OS Boot", "Diagnostic boot completed" },
	{ 0x1f, 0x04, "OS Boot", "CD-ROM boot completed" },
	{ 0x1f, 0x05, "OS Boot", "ROM boot completed" },
	{ 0x1f, 0x06, "OS Boot", "boot completed - device not specified" },

	{ 0x20, 0x00, "OS Critical Stop", "Stop during OS load/init" },
	{ 0x20, 0x01, "OS Critical Stop", "Run-time stop" },

	{ 0x21, 0x00, "Slot/Connector", "Fault Status asserted" },
	{ 0x21, 0x01, "Slot/Connector", "Identify Status asserted" },
	{ 0x21, 0x02, "Slot/Connector", "Slot/Connector Device installed/attached" },
	{ 0x21, 0x03, "Slot/Connector", "Slot/Connector ready for device installation" },
	{ 0x21, 0x04, "Slot/Connector", "Slot/Connector ready for device removal" },
	{ 0x21, 0x05, "Slot/Connector", "Slot Power is off" },
	{ 0x21, 0x06, "Slot/Connector", "Slot/Connector device removal request" },
	{ 0x21, 0x07, "Slot/Connector", "Interlock asserted" },
	{ 0x21, 0x08, "Slot/Connector", "Slot is disabled" },

	{ 0x22, 0x00, "System ACPI Power State", "S0/G0: working" },
	{ 0x22, 0x01, "System ACPI Power State", "S1: sleeping with system hw & processor context maintained" },
	{ 0x22, 0x02, "System ACPI Power State", "S2: sleeping, processor context lost" },
	{ 0x22, 0x03, "System ACPI Power State", "S3: sleeping, processor & hw context lost, memory retained" },
	{ 0x22, 0x04, "System ACPI Power State", "S4: non-volatile sleep/suspend-to-disk" },
	{ 0x22, 0x05, "System ACPI Power State", "S5/G2: soft-off" },
	{ 0x22, 0x06, "System ACPI Power State", "S4/S5: soft-off" },
	{ 0x22, 0x07, "System ACPI Power State", "G3: mechanical off" },
	{ 0x22, 0x08, "System ACPI Power State", "Sleeping in S1/S2/S3 state" },
	{ 0x22, 0x09, "System ACPI Power State", "G1: sleeping" },
	{ 0x22, 0x0a, "System ACPI Power State", "S5: entered by override" },
	{ 0x22, 0x0b, "System ACPI Power State", "Legacy ON state" },
	{ 0x22, 0x0c, "System ACPI Power State", "Legacy OFF state" },
	{ 0x22, 0x0e, "System ACPI Power State", "Unknown" },

	{ 0x23, 0x00, "Watchdog 2", "Timer expired" },
	{ 0x23, 0x01, "Watchdog 2", "Hard reset" },
	{ 0x23, 0x02, "Watchdog 2", "Power down" },
	{ 0x23, 0x03, "Watchdog 2", "Power cycle" },
	{ 0x23, 0x04, "Watchdog 2", "reserved" },
	{ 0x23, 0x05, "Watchdog 2", "reserved" },
	{ 0x23, 0x06, "Watchdog 2", "reserved" },
	{ 0x23, 0x07, "Watchdog 2", "reserved" },
	{ 0x23, 0x08, "Watchdog 2", "Timer interrupt" },

	{ 0x24, 0x00, "Platform Alert", "Platform generated page" },
	{ 0x24, 0x01, "Platform Alert", "Platform generated LAN alert" },
	{ 0x24, 0x02, "Platform Alert", "Platform Event Trap generated" },
	{ 0x24, 0x03, "Platform Alert", "Platform generated SNMP trap, OEM format" },

	{ 0x25, 0x00, "Entity Presence", "Present" },
	{ 0x25, 0x01, "Entity Presence", "Absent" },
	{ 0x25, 0x02, "Entity Presence", "Disabled" },

	{ 0x26, 0x00, "Monitor ASIC/IC", NULL },

	{ 0x27, 0x00, "LAN", "Heartbeat Lost" },
	{ 0x27, 0x01, "LAN", "Heartbeat" },

	{ 0x28, 0x00, "Management Subsystem Health", "Sensor access degraded or unavailable" },
	{ 0x28, 0x01, "Management Subsystem Health", "Controller access degraded or unavailable" },
	{ 0x28, 0x02, "Management Subsystem Health", "Management controller off-line" },
	{ 0x28, 0x03, "Management Subsystem Health", "Management controller unavailable" },

	{ 0x29, 0x00, "Battery", "Low" },
	{ 0x29, 0x01, "Battery", "Failed" },
	{ 0x29, 0x02, "Battery", "Presence Detected" },

	{ 0x00, 0x00, NULL, NULL },
};

int ipmi_sel_main(struct ipmi_intf *, int, char **);

#endif /*_IPMI_SEL_H*/
