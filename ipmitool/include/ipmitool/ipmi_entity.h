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

#ifndef IPMI_ENTITY_H
#define IPMI_ENTITY_H

#include <ipmitool/helper.h>

const struct valstr entity_id_vals[] __attribute__((unused)) = {
	{ 0x00, "Unspecified" },
	{ 0x01, "Other" },
	{ 0x02, "Unknown" },
	{ 0x03, "Processor" },
	{ 0x04, "Disk or Disk Bay" },
	{ 0x05, "Peripheral Bay" },
	{ 0x06, "System Management Module" },
	{ 0x07, "System Board" },
	{ 0x08, "Memory Module" },
	{ 0x09, "Processor Module" },
	{ 0x0a, "Power Supply" },
	{ 0x0b, "Add-in Card" },
	{ 0x0c, "Front Panel Board" },
	{ 0x0d, "Back Panel Board" },
	{ 0x0e, "Power System Board" },
	{ 0x0f, "Drive Backplane" },
	{ 0x10, "System Internal Expansion Board" },
	{ 0x11, "Other System Board" },
	{ 0x12, "Processor Board" },
	{ 0x13, "Power Unit" },
	{ 0x14, "Power Module" },
	{ 0x15, "Power Management" },
	{ 0x16, "Chassis Back Panel Board" },
	{ 0x17, "System Chassis" },
	{ 0x18, "Sub-Chassis" },
	{ 0x19, "Other Chassis Board" },
	{ 0x1a, "Disk Drive Bay" },
	{ 0x1b, "Peripheral Bay" },
	{ 0x1c, "Device Bay" },
	{ 0x1d, "Fan Device" },
	{ 0x1e, "Cooling Unit" },
	{ 0x1f, "Cable/Interconnect" },
	{ 0x20, "Memory Device" },
	{ 0x21, "System Management Software" },
	{ 0x22, "BIOS" },
	{ 0x23, "Operating System" },
	{ 0x24, "System Bus" },
	{ 0x25, "Group" },
	{ 0x26, "Remote Management Device" },
	{ 0x27, "External Environment" },
	{ 0x28, "Battery" },
	{ 0x00, NULL },
};

const struct valstr device_type_vals[] __attribute__((unused)) = {
	{ 0x00, "Reserved" },
	{ 0x01, "Reserved" },
	{ 0x02, "DS1624 temperature sensor" },
	{ 0x03, "DS1621 temperature sensor" },
	{ 0x04, "LM75 Temperature Sensor" },
	{ 0x05, "Heceta ASIC" },
	{ 0x06, "Reserved" },
	{ 0x07, "Reserved" },
	{ 0x08, "EEPROM, 24C01" },
	{ 0x09, "EEPROM, 24C02" },
	{ 0x0a, "EEPROM, 24C04" },
	{ 0x0b, "EEPROM, 24C08" },
	{ 0x0c, "EEPROM, 24C16" },
	{ 0x0d, "EEPROM, 24C17" },
	{ 0x0e, "EEPROM, 24C32" },
	{ 0x0f, "EEPROM, 24C64" },
	{ 0x1000, "IPMI FRU Inventory" },
	{ 0x1001, "DIMM Memory ID" },
	{ 0x1002, "IPMI FRU Inventory" },
	{ 0x1003, "System Processor Cartridge FRU" },
	{ 0x11, "Reserved" },
	{ 0x12, "Reserved" },
	{ 0x13, "Reserved" },
	{ 0x14, "PCF 8570 256 byte RAM" },
	{ 0x15, "PCF 8573 clock/calendar" },
	{ 0x16, "PCF 8574A I/O Port" },
	{ 0x17, "PCF 8583 clock/calendar" },
	{ 0x18, "PCF 8593 clock/calendar" },
	{ 0x19, "Clock calendar" },
	{ 0x1a, "PCF 8591 A/D, D/A Converter" },
	{ 0x1b, "I/O Port" },
	{ 0x1c, "A/D Converter" },
	{ 0x1d, "D/A Converter" },
	{ 0x1e, "A/D, D/A Converter" },
	{ 0x1f, "LCD Controler/Driver" },
	{ 0x20, "Core Logic (Chip set) Device" },
	{ 0x21, "LMC6874 Intelligent Battery controller" },
	{ 0x22, "Intelligent Batter controller" },
	{ 0x23, "Combo Management ASIC" },
	{ 0x24, "Maxim 1617 Temperature Sensor" },
	{ 0xbf, "Other/Unspecified" },
	{ 0x00, NULL },
};

#endif /* IPMI_ENTITY_H */

