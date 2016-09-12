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

#include <stddef.h>
#include <ipmitool/ipmi_strings.h>
#include <ipmitool/ipmi_constants.h>
#include <ipmitool/ipmi_sensor.h>
#include <ipmitool/ipmi_sel.h>  /* for IPMI_OEM */

const struct valstr ipmi_oem_info[] = {

   { IPMI_OEM_UNKNOWN,                "Unknown" },
   { IPMI_OEM_HP,                     "Hewlett-Packard" },
   { IPMI_OEM_SUN,                    "Sun Microsystems" },
   { IPMI_OEM_INTEL,                  "Intel Corporation" },
   { IPMI_OEM_LMC,                    "LMC" },
   { IPMI_OEM_RADISYS,                "RadiSys Corporation" },
   { IPMI_OEM_TYAN,                   "Tyan Computer Corporation" },
   { IPMI_OEM_NEWISYS,                "Newisys" },
   { IPMI_OEM_SUPERMICRO,             "Supermicro" },
   { IPMI_OEM_GOOGLE,                 "Google" },
   { IPMI_OEM_KONTRON,                "Kontron" },
   { IPMI_OEM_NOKIA,                  "Nokia" },
   { IPMI_OEM_PICMG,                  "PICMG" },
   { IPMI_OEM_PEPPERCON,              "Peppercon AG" },
   { IPMI_OEM_DELL,                   "DELL Inc" },
   { IPMI_OEM_NEC,                    "NEC" },
   { IPMI_OEM_MAGNUM,                 "Magnum Technologies" },
   { IPMI_OEM_FUJITSU_SIEMENS,        "Fujitsu Siemens" },
   { IPMI_OEM_TATUNG,                 "Tatung" },
   { IPMI_OEM_AMI,                    "AMI" },
   { IPMI_OEM_RARITAN,                "Raritan" },
   { IPMI_OEM_AVOCENT,                "Avocent" },
   { IPMI_OEM_OSA,                    "OSA" },
   { IPMI_OEM_TOSHIBA,                "Toshiba" },
   { IPMI_OEM_HITACHI_116,            "Hitachi" },
   { IPMI_OEM_HITACHI_399,            "Hitachi" },
   { IPMI_OEM_NOKIA_SOLUTIONS_AND_NETWORKS, "Nokia Solutions and Networks" },
   { IPMI_OEM_BULL,                   "Bull Company" },
   { IPMI_OEM_PPS,                    "Pigeon Point Systems" },
   { IPMI_OEM_BROADCOM,               "Broadcom Corporation" },
   { IPMI_OEM_ERICSSON,               "Ericsson AB"},
   { IPMI_OEM_QUANTA,                 "Quanta" },
   { IPMI_OEM_VITA,                   "VITA" },
   { IPMI_OEM_ADVANTECH,              "Advantech" },
   /************************************************************************
    * Add ID String for IANA Enterprise Number of IBM & ADLINK
    * https://www.iana.org/assignments/enterprise-numbers/enterprise-numbers
    *  2
    *    IBM
    *      Kristine Adamson
    *        adamson&us.ibm.com
    *  4769
    *    IBM Corporation
    *      Victor Sample
    *        vsample&us.ibm.com
    *  20301
    *    IBM eServer X
    *      Lynn Fore
    *        sls&us.ibm.com
    *  24339
    *    ADLINK TECHNOLOGY INC.
    *      Ryan Hsu
    *        ryan.hsu&adlinktech.com
    ************************************************************************/
   { IPMI_OEM_IBM_2,                  "IBM" },
   { IPMI_OEM_IBM_4769,               "IBM Corporation" },
   { IPMI_OEM_IBM_20301,              "IBM eServer X" },
   { IPMI_OEM_ADLINK_24339,           "ADLINK Technology Inc." },
   { 0xffff , NULL },
};

const struct oemvalstr ipmi_oem_product_info[] = {
   /* Keep OEM grouped together */
   /* Intel stuff, thanks to Tim Bell */
   { IPMI_OEM_INTEL, 0x000C, "TSRLT2" },
   { IPMI_OEM_INTEL, 0x001B, "TIGPR2U" },
   { IPMI_OEM_INTEL, 0x0022, "TIGI2U" },
   { IPMI_OEM_INTEL, 0x0026, "Bridgeport" },
   { IPMI_OEM_INTEL, 0x0028, "S5000PAL" },
   { IPMI_OEM_INTEL, 0x0029, "S5000PSL" },
   { IPMI_OEM_INTEL, 0x0100, "Tiger4" },
   { IPMI_OEM_INTEL, 0x0103, "McCarran" },
   { IPMI_OEM_INTEL, 0x0800, "ZT5504" },
   { IPMI_OEM_INTEL, 0x0808, "MPCBL0001" },
   { IPMI_OEM_INTEL, 0x0811, "TIGW1U" },
   { IPMI_OEM_INTEL, 0x4311, "NSI2U" },
   /* Kontron */
   { IPMI_OEM_KONTRON,4000, "AM4000 AdvancedMC" },
   { IPMI_OEM_KONTRON,4001, "AM4001 AdvancedMC" },
   { IPMI_OEM_KONTRON,4002, "AM4002 AdvancedMC" },
   { IPMI_OEM_KONTRON,4010, "AM4010 AdvancedMC" },
   { IPMI_OEM_KONTRON,5503, "AM4500/4520 AdvancedMC" },
   { IPMI_OEM_KONTRON,5504, "AM4300 AdvancedMC" },
   { IPMI_OEM_KONTRON,5507, "AM4301 AdvancedMC" },
   { IPMI_OEM_KONTRON,5508, "AM4330 AdvancedMC" },
   { IPMI_OEM_KONTRON,5520, "KTC5520/EATX" },
   { IPMI_OEM_KONTRON,5703, "RTM8020" },
   { IPMI_OEM_KONTRON,5704, "RTM8030" },
   { IPMI_OEM_KONTRON,5705, "RTM8050" },
   { IPMI_OEM_KONTRON,6000, "CP6000" },
   { IPMI_OEM_KONTRON,6006, "DT-64" },
   { IPMI_OEM_KONTRON,6010, "CP6010" },
   { IPMI_OEM_KONTRON,6011, "CP6011" },
   { IPMI_OEM_KONTRON,6012, "CP6012" },
   { IPMI_OEM_KONTRON,6014, "CP6014" },
   { IPMI_OEM_KONTRON,5002, "AT8001" },
   { IPMI_OEM_KONTRON,5003, "AT8010" },
   { IPMI_OEM_KONTRON,5004, "AT8020" },
   { IPMI_OEM_KONTRON,5006, "AT8030 IPMC" },
   { IPMI_OEM_KONTRON,2025, "AT8030 MMC" },
   { IPMI_OEM_KONTRON,5007, "AT8050" },
   { IPMI_OEM_KONTRON,5301, "AT8400" },
   { IPMI_OEM_KONTRON,5303, "AT8901" },
   /* Broadcom */
   { IPMI_OEM_BROADCOM, 5725, "BCM5725" },
   /* Ericsson */
   { IPMI_OEM_ERICSSON, 0x0054, "Phantom" },
   /* Advantech */
   /* ATCA Blades */
   { IPMI_OEM_ADVANTECH, 0x3393, "MIC-3393" },
   { IPMI_OEM_ADVANTECH, 0x3395, "MIC-3395" },
   { IPMI_OEM_ADVANTECH, 0x3396, "MIC-3396" },
   { IPMI_OEM_ADVANTECH, 0x5302, "MIC-5302" },
   { IPMI_OEM_ADVANTECH, 0x5304, "MIC-5304" },
   { IPMI_OEM_ADVANTECH, 0x5320, "MIC-5320" },
   { IPMI_OEM_ADVANTECH, 0x5321, "MIC-5321" },
   { IPMI_OEM_ADVANTECH, 0x5322, "MIC-5322" },
   { IPMI_OEM_ADVANTECH, 0x5332, "MIC-5332" },
   { IPMI_OEM_ADVANTECH, 0x5333, "MIC-5333" },
   { IPMI_OEM_ADVANTECH, 0x5342, "MIC-5342" },
   { IPMI_OEM_ADVANTECH, 0x5343, "MIC-5343" },
   { IPMI_OEM_ADVANTECH, 0x5344, "MIC-5344" },
   { IPMI_OEM_ADVANTECH, 0x5345, "MIC-5345" },
   { IPMI_OEM_ADVANTECH, 0x5201, "MIC-5201 Dual 10GE AMC"},
   { IPMI_OEM_ADVANTECH, 0x5203, "MIC-5203 Quad GbE AMC"},
   { IPMI_OEM_ADVANTECH, 0x5212, "MIC-5212 Dual 10GE AMC"},
   /* AdvancedMC */
   { IPMI_OEM_ADVANTECH, 0x5401, "MIC-5401" },
   { IPMI_OEM_ADVANTECH, 0x5601, "MIC-5601" },
   { IPMI_OEM_ADVANTECH, 0x5602, "MIC-5602" },
   { IPMI_OEM_ADVANTECH, 0x5604, "MIC-5604" },
   { IPMI_OEM_ADVANTECH, 0x5603, "MIC-5603" },
   { IPMI_OEM_ADVANTECH, 0x6311, "MIC-6311" },
   { IPMI_OEM_ADVANTECH, 0x6313, "MIC-6313" },
   { IPMI_OEM_ADVANTECH, 0x8301, "MIC-8301" },
   { IPMI_OEM_ADVANTECH, 0x8302, "MIC-8302" },
   { IPMI_OEM_ADVANTECH, 0x8304, "MIC-8304" },
   { IPMI_OEM_ADVANTECH, 0x5101, "RTM-5101" },
   { IPMI_OEM_ADVANTECH, 0x5102, "RTM-5102" },
   { IPMI_OEM_ADVANTECH, 0x5106, "RTM-5106" },
   { IPMI_OEM_ADVANTECH, 0x5107, "RTM-5107" },
   { IPMI_OEM_ADVANTECH, 0x5210, "RTM-5210" },
   { IPMI_OEM_ADVANTECH, 0x5220, "RTM-5220" },
   { IPMI_OEM_ADVANTECH, 0x5104, "RTM-5104" },
   { IPMI_OEM_ADVANTECH, 0x5500, "UTCA-5500"},
   { IPMI_OEM_ADVANTECH, 0x5503, "UTCA-5503"},
   { IPMI_OEM_ADVANTECH, 0x5504, "UTCA-5504"},
   { IPMI_OEM_ADVANTECH, 0x5801, "UTCA-5801"},
   { IPMI_OEM_ADVANTECH, 0x2210, "NCPB-2210"},
   { IPMI_OEM_ADVANTECH, 0x2305, "NCPB-2305"},
   { IPMI_OEM_ADVANTECH, 0x2320, "NCPB-2320"},
   { IPMI_OEM_ADVANTECH, 0x3109, "NCP-3109" },
   { IPMI_OEM_ADVANTECH, 0x3110, "NCP-3110" },
   { IPMI_OEM_ADVANTECH, 0x3200, "NCP-3200" },
   { IPMI_OEM_ADVANTECH, 0x5060, "SMM-5060" },
   { IPMI_OEM_ADVANTECH, 0x3210, "FWA-3210" },
   { IPMI_OEM_ADVANTECH, 0x3220, "FWA-3220" },
   { IPMI_OEM_ADVANTECH, 0x3221, "FWA-3221" },
   { IPMI_OEM_ADVANTECH, 0x3230, "FWA-3230" },
   { IPMI_OEM_ADVANTECH, 0x3231, "FWA-3231" },
   { IPMI_OEM_ADVANTECH, 0x3233, "FWA-3233" },
   { IPMI_OEM_ADVANTECH, 0x3250, "FWA-3250" },
   { IPMI_OEM_ADVANTECH, 0x3260, "FWA-3260" },
   { IPMI_OEM_ADVANTECH, 0x5020, "FWA-5020" },
   { IPMI_OEM_ADVANTECH, 0x6510, "FWA-6510" },
   { IPMI_OEM_ADVANTECH, 0x6511, "FWA-6511" },
   { IPMI_OEM_ADVANTECH, 0x6512, "FWA-6512" },
   { IPMI_OEM_ADVANTECH, 0x6520, "FWA-6520" },
   { IPMI_OEM_ADVANTECH, 0x6521, "FWA-6521" },
   { IPMI_OEM_ADVANTECH, 0x6522, "FWA-6522" },
   { IPMI_OEM_ADVANTECH, 0x7310, "ATCA-7310"},
   { IPMI_OEM_ADVANTECH, 0x7330, "ATCA-7330"},
   { IPMI_OEM_ADVANTECH, 0x7410, "ATCA-7410"},
   { IPMI_OEM_ADVANTECH, 0x9023, "ATCA-9023"},
   { IPMI_OEM_ADVANTECH, 0x9112, "ATCA-9112"},
   { IPMI_OEM_ADVANTECH, 0x4201, "AMC-4201" },
   { IPMI_OEM_ADVANTECH, 0x4202, "AMC-4202" },
   { IPMI_OEM_ADVANTECH, 0x3211, "NAMB-3211"},
   { IPMI_OEM_ADVANTECH, 0x1207, "CPCI-1207"},
   { IPMI_OEM_ADVANTECH, 0x120E, "CPCI-1207 Test Board"},
   { IPMI_OEM_ADVANTECH, 0x1304, "CPCI-1304"},
   { IPMI_OEM_ADVANTECH, 0x7001, "CPCI-7001"},
   { IPMI_OEM_ADVANTECH, 0x8220, "CPCI-8220"},
   { IPMI_OEM_ADVANTECH, 0x9001, "ESP-9001" },
   { IPMI_OEM_ADVANTECH, 0x9002, "ESP-9002" },
   { IPMI_OEM_ADVANTECH, 0x9012, "ESP-9012" },
   { IPMI_OEM_ADVANTECH, 0x9212, "ESP-9212" },
   { IPMI_OEM_ADVANTECH, 0x6000, "CGS-6000" },
   { IPMI_OEM_ADVANTECH, 0x6010, "CGS-6010" },
   /* ADLINK Technology Inc. */
   /* AdvancedTCA Processor Blades */
   { IPMI_OEM_ADLINK_24339, 0x3100, "aTCA-3100" },
   { IPMI_OEM_ADLINK_24339, 0x3110, "aTCA-3110" },
   { IPMI_OEM_ADLINK_24339, 0x3150, "aTCA-3150" },
   { IPMI_OEM_ADLINK_24339, 0x3420, "aTCA-3420" },
   { IPMI_OEM_ADLINK_24339, 0x3710, "aTCA-3710" },
   { IPMI_OEM_ADLINK_24339, 0x6100, "aTCA-6100" },
   { IPMI_OEM_ADLINK_24339, 0x6200, "aTCA-6200" },
   { IPMI_OEM_ADLINK_24339, 0x6250, "aTCA-6250/6250STW" },
   { IPMI_OEM_ADLINK_24339, 0x6270, "aTCA-R6270" },
   { IPMI_OEM_ADLINK_24339, 0x6280, "aTCA-R6280" },
   { IPMI_OEM_ADLINK_24339, 0x6890, "aTCA-6890" },
   { IPMI_OEM_ADLINK_24339, 0x6891, "aTCA-6891" },
   { IPMI_OEM_ADLINK_24339, 0x6900, "aTCA-6900" },
   { IPMI_OEM_ADLINK_24339, 0x6905, "aTCA-R6905" },
   { IPMI_OEM_ADLINK_24339, 0x690A, "aTCA-R6900" },
   { IPMI_OEM_ADLINK_24339, 0x8214, "aTCA-8214" },
   { IPMI_OEM_ADLINK_24339, 0x8606, "aTCA-8606" },
   { IPMI_OEM_ADLINK_24339, 0x9300, "aTCA-9300" },
   { IPMI_OEM_ADLINK_24339, 0x9700, "aTCA-9700" },
   { IPMI_OEM_ADLINK_24339, 0x9700, "aTCA-R9700" },
   { IPMI_OEM_ADLINK_24339, 0x970D, "aTCA-9700D" },
   { IPMI_OEM_ADLINK_24339, 0x9710, "aTCA-9710" },
   { IPMI_OEM_ADLINK_24339, 0x9710, "aTCA-R9710" },
   { IPMI_OEM_ADLINK_24339, 0xF001, "aTCA-FN001" },
   { IPMI_OEM_ADLINK_24339, 0xF2A0, "aTCA-F2AX" },
   { IPMI_OEM_ADLINK_24339, 0xF5A0, "aTCA-F5AX" },
   /* CompactPCI Blades */
   { IPMI_OEM_ADLINK_24339, 0x3510, "cPCI-3510" },
   { IPMI_OEM_ADLINK_24339, 0x3970, "cPCI-3970" },
   { IPMI_OEM_ADLINK_24339, 0x6010, "cPCI-6010" },
   { IPMI_OEM_ADLINK_24339, 0x6210, "cPCI-6210" },
   { IPMI_OEM_ADLINK_24339, 0x6510, "cPCI-6510" },
   { IPMI_OEM_ADLINK_24339, 0x6520, "cPCI-6520" },
   { IPMI_OEM_ADLINK_24339, 0x6525, "cPCI-6525" },
   { IPMI_OEM_ADLINK_24339, 0x6530, "cPCI-6530/6530BL" },
   { IPMI_OEM_ADLINK_24339, 0x6600, "cPCI-6600" },
   { IPMI_OEM_ADLINK_24339, 0x6840, "cPCI-6840" },
   { IPMI_OEM_ADLINK_24339, 0x6870, "cPCI-6870" },
   { IPMI_OEM_ADLINK_24339, 0x6880, "cPCI-6880" },
   { IPMI_OEM_ADLINK_24339, 0x6910, "cPCI-6910" },
   { IPMI_OEM_ADLINK_24339, 0x6920, "cPCI-6920" },
   { IPMI_OEM_ADLINK_24339, 0x6930, "cPCI-6930" },
   { IPMI_OEM_ADLINK_24339, 0x6940, "cPCI-6940" },
   /* VPX Blades */
   { IPMI_OEM_ADLINK_24339, 0x3000, "VPX3000" },
   { IPMI_OEM_ADLINK_24339, 0x3001, "VPX3001" },
   { IPMI_OEM_ADLINK_24339, 0x3002, "VPX3002" },
   { IPMI_OEM_ADLINK_24339, 0x3010, "VPX3010" },
   { IPMI_OEM_ADLINK_24339, 0x3F10, "VPX3G10" },
   { IPMI_OEM_ADLINK_24339, 0x6000, "VPX6000" },
   /* Network Appliance */
   { IPMI_OEM_ADLINK_24339, 0x0410, "MXN-0410" },
   { IPMI_OEM_ADLINK_24339, 0x2600, "MCN-2600" },
   { IPMI_OEM_ADLINK_24339, 0x1500, "MCN-1500" },

   { 0xffffff        , 0xffff , NULL },
 };

const char *ipmi_generic_sensor_type_vals[] = {
    "reserved",
    "Temperature", "Voltage", "Current", "Fan",
    "Physical Security", "Platform Security", "Processor",
    "Power Supply", "Power Unit", "Cooling Device", "Other",
    "Memory", "Drive Slot / Bay", "POST Memory Resize",
    "System Firmwares", "Event Logging Disabled", "Watchdog1",
    "System Event", "Critical Interrupt", "Button",
    "Module / Board", "Microcontroller", "Add-in Card",
    "Chassis", "Chip Set", "Other FRU", "Cable / Interconnect",
    "Terminator", "System Boot Initiated", "Boot Error",
    "OS Boot", "OS Critical Stop", "Slot / Connector",
    "System ACPI Power State", "Watchdog2", "Platform Alert",
    "Entity Presence", "Monitor ASIC", "LAN",
    "Management Subsys Health", "Battery", "Session Audit",
    "Version Change", "FRU State",
    NULL
};

const struct oemvalstr ipmi_oem_sensor_type_vals[] = {
   /* Keep OEM grouped together */
   { IPMI_OEM_KONTRON, 0xC0, "Firmware Info" },
   { IPMI_OEM_KONTRON, 0xC2, "Init Agent" },
   { IPMI_OEM_KONTRON, 0xC2, "Board Reset(cPCI)" },
   { IPMI_OEM_KONTRON, 0xC3, "IPMBL Link State" },
   { IPMI_OEM_KONTRON, 0xC4, "Board Reset" },
   { IPMI_OEM_KONTRON, 0xC5, "FRU Information Agent" },
   { IPMI_OEM_KONTRON, 0xC6, "POST Value Sensor" },
   { IPMI_OEM_KONTRON, 0xC7, "FWUM Status" },
   { IPMI_OEM_KONTRON, 0xC8, "Switch Mngt Software Status" },
   { IPMI_OEM_KONTRON, 0xC9, "OEM Diagnostic Status" },
   { IPMI_OEM_KONTRON, 0xCA, "Component Firmware Upgrade" },
   { IPMI_OEM_KONTRON, 0xCB, "FRU Over Current" },
   { IPMI_OEM_KONTRON, 0xCC, "FRU Sensor Error" },
   { IPMI_OEM_KONTRON, 0xCD, "FRU Power Denied" },
   { IPMI_OEM_KONTRON, 0xCE, "Reserved" },
   { IPMI_OEM_KONTRON, 0xCF, "Board Reset" },
   { IPMI_OEM_KONTRON, 0xD0, "Clock Resource Control" },
   { IPMI_OEM_KONTRON, 0xD1, "Power State" },
   { IPMI_OEM_KONTRON, 0xD2, "FRU Mngt Power Failure" },
   { IPMI_OEM_KONTRON, 0xD3, "Jumper Status" },
   { IPMI_OEM_KONTRON, 0xF2, "RTM Module Hotswap" },
   /* PICMG Sensor Types */
   { IPMI_OEM_PICMG, 0xF0, "FRU Hot Swap" },
   { IPMI_OEM_PICMG, 0xF1,"IPMB Physical Link" },
   { IPMI_OEM_PICMG, 0xF2, "Module Hot Swap" },
   { IPMI_OEM_PICMG, 0xF3, "Power Channel Notification" },
   { IPMI_OEM_PICMG, 0xF4, "Telco Alarm Input" },
   /* VITA 46.11 Sensor Types */
   { IPMI_OEM_VITA, 0xF0, "FRU State" },
   { IPMI_OEM_VITA, 0xF1, "System IPMB Link" },
   { IPMI_OEM_VITA, 0xF2, "FRU Health" },
   { IPMI_OEM_VITA, 0xF3, "FRU Temperature" },
   { IPMI_OEM_VITA, 0xF4, "Payload Test Results" },
   { IPMI_OEM_VITA, 0xF5, "Payload Test Status" },

   { 0xffffff,      0x00,  NULL }
};

const struct valstr ipmi_netfn_vals[] = {
	{ IPMI_NETFN_CHASSIS,	"Chassis" },
	{ IPMI_NETFN_BRIDGE,	"Bridge" },
	{ IPMI_NETFN_SE,	"SensorEvent" },
	{ IPMI_NETFN_APP,	"Application" },
	{ IPMI_NETFN_FIRMWARE,	"Firmware" },
	{ IPMI_NETFN_STORAGE,	"Storage" },
	{ IPMI_NETFN_TRANSPORT,	"Transport" },
	{ 0xff,			NULL },
};

/*
 * From table 26-4 of the IPMI v2 specification
 */
const struct valstr ipmi_bit_rate_vals[] = {
	{ 0x00, "IPMI-Over-Serial-Setting"}, /* Using the value in the IPMI Over Serial Config */
	{ 0x06, "9.6" },
	{ 0x07, "19.2" },
	{ 0x08, "38.4" },
	{ 0x09, "57.6" },
	{ 0x0A, "115.2" },
	{ 0x00, NULL },
};

const struct valstr ipmi_channel_activity_type_vals[] = {
	{ 0, "IPMI Messaging session active" },
	{ 1, "Callback Messaging session active" },
	{ 2, "Dial-out Alert active" },
	{ 3, "TAP Page Active" },
	{ 0x00, NULL },
};


const struct valstr ipmi_privlvl_vals[] = {
	{ IPMI_SESSION_PRIV_CALLBACK,   "CALLBACK" },
	{ IPMI_SESSION_PRIV_USER,    	"USER" },
	{ IPMI_SESSION_PRIV_OPERATOR,	"OPERATOR" },
	{ IPMI_SESSION_PRIV_ADMIN,	    "ADMINISTRATOR" },
	{ IPMI_SESSION_PRIV_OEM,    	"OEM" },
	{ 0xF,	        		    	"NO ACCESS" },
	{ 0xFF,			             	NULL },
};


const struct valstr ipmi_set_in_progress_vals[] = {
	{ IPMI_SET_IN_PROGRESS_SET_COMPLETE, "set-complete"    },
	{ IPMI_SET_IN_PROGRESS_IN_PROGRESS,  "set-in-progress" },
	{ IPMI_SET_IN_PROGRESS_COMMIT_WRITE, "commit-write"    },
	{ 0,                            NULL },
};


const struct valstr ipmi_authtype_session_vals[] = {
	{ IPMI_SESSION_AUTHTYPE_NONE,     "NONE" },
	{ IPMI_SESSION_AUTHTYPE_MD2,      "MD2" },
	{ IPMI_SESSION_AUTHTYPE_MD5,      "MD5" },
	{ IPMI_SESSION_AUTHTYPE_PASSWORD, "PASSWORD" },
	{ IPMI_SESSION_AUTHTYPE_OEM,      "OEM" },
	{ IPMI_SESSION_AUTHTYPE_RMCP_PLUS,"RMCP+" },
	{ 0xFF,                           NULL },
};


const struct valstr ipmi_authtype_vals[] = {
	{ IPMI_1_5_AUTH_TYPE_BIT_NONE,     "NONE" },
	{ IPMI_1_5_AUTH_TYPE_BIT_MD2,      "MD2" },
	{ IPMI_1_5_AUTH_TYPE_BIT_MD5,      "MD5" },
	{ IPMI_1_5_AUTH_TYPE_BIT_PASSWORD, "PASSWORD" },
	{ IPMI_1_5_AUTH_TYPE_BIT_OEM,      "OEM" },
	{ 0,                               NULL },
};

const struct valstr entity_id_vals[] = {
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
	{ 0x29, "Processing Blade" },
	{ 0x2A, "Connectivity Switch" },
	{ 0x2B, "Processor/Memory Module" },
	{ 0x2C, "I/O Module" },
	{ 0x2D, "Processor/IO Module" },
	{ 0x2E, "Management Controller Firmware" },
	{ 0x2F, "IPMI Channel" },
	{ 0x30, "PCI Bus" },
	{ 0x31, "PCI Express Bus" },
	{ 0x32, "SCSI Bus (parallel)" },
	{ 0x33, "SATA/SAS Bus" },
	{ 0x34, "Processor/Front-Side Bus" },
	{ 0x35, "Real Time Clock(RTC)" },
	{ 0x36, "Reserved" },
	{ 0x37, "Air Inlet" },
	{ 0x38, "Reserved" },
	{ 0x39, "Reserved" },
	{ 0x3A, "Reserved" },
	{ 0x3B, "Reserved" },
	{ 0x3C, "Reserved" },
	{ 0x3D, "Reserved" },
	{ 0x3E, "Reserved" },
	{ 0x3F, "Reserved" },
	{ 0x40, "Air Inlet" },
	{ 0x41, "Processor" },
	{ 0x42, "Baseboard/Main System Board" },
	/* PICMG */
	{ 0xA0, "PICMG Front Board" },
	{ 0xC0, "PICMG Rear Transition Module" },
	{ 0xC1, "PICMG AdvancedMC Module" },
	{ 0xF0, "PICMG Shelf Management Controller" },
	{ 0xF1, "PICMG Filtration Unit" },
	{ 0xF2, "PICMG Shelf FRU Information" },
	{ 0xF3, "PICMG Alarm Panel" },
	{ 0x00, NULL },
};

const struct valstr entity_device_type_vals[] = {
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
	{ 0x1f, "LCD Controller/Driver" },
	{ 0x20, "Core Logic (Chip set) Device" },
	{ 0x21, "LMC6874 Intelligent Battery controller" },
	{ 0x22, "Intelligent Batter controller" },
	{ 0x23, "Combo Management ASIC" },
	{ 0x24, "Maxim 1617 Temperature Sensor" },
	{ 0xbf, "Other/Unspecified" },
	{ 0x00, NULL },
};

const struct valstr ipmi_channel_protocol_vals[] = {
	{ 0x00, "reserved" },
	{ 0x01, "IPMB-1.0" },
	{ 0x02, "ICMB-1.0" },
	{ 0x03, "reserved" },
	{ 0x04, "IPMI-SMBus" },
	{ 0x05, "KCS" },
	{ 0x06, "SMIC" },
	{ 0x07, "BT-10" },
	{ 0x08, "BT-15" },
	{ 0x09, "TMode" },
	{ 0x1c, "OEM 1" },
	{ 0x1d, "OEM 2" },
	{ 0x1e, "OEM 3" },
	{ 0x1f, "OEM 4" },
	{ 0x00, NULL },
};


const struct valstr ipmi_channel_medium_vals[] = {
	{ IPMI_CHANNEL_MEDIUM_RESERVED,	"reserved" },
	{ IPMI_CHANNEL_MEDIUM_IPMB_I2C,	"IPMB (I2C)" },
	{ IPMI_CHANNEL_MEDIUM_ICMB_1,	"ICMB v1.0" },
	{ IPMI_CHANNEL_MEDIUM_ICMB_09,	"ICMB v0.9" },
	{ IPMI_CHANNEL_MEDIUM_LAN,	"802.3 LAN" },
	{ IPMI_CHANNEL_MEDIUM_SERIAL,	"Serial/Modem" },
	{ IPMI_CHANNEL_MEDIUM_LAN_OTHER,"Other LAN" },
	{ IPMI_CHANNEL_MEDIUM_SMBUS_PCI,"PCI SMBus" },
	{ IPMI_CHANNEL_MEDIUM_SMBUS_1,	"SMBus v1.0/v1.1" },
	{ IPMI_CHANNEL_MEDIUM_SMBUS_2,	"SMBus v2.0" },
	{ IPMI_CHANNEL_MEDIUM_USB_1,	"USB 1.x" },
	{ IPMI_CHANNEL_MEDIUM_USB_2,	"USB 2.x" },
	{ IPMI_CHANNEL_MEDIUM_SYSTEM,	"System Interface" },
	{ 0x00, NULL },
};

const struct valstr completion_code_vals[] = {
	{ 0x00, "Command completed normally" },
	{ 0xc0, "Node busy" },
	{ 0xc1, "Invalid command" },
	{ 0xc2, "Invalid command on LUN" },
	{ 0xc3, "Timeout" },
	{ 0xc4, "Out of space" },
	{ 0xc5, "Reservation cancelled or invalid" },
	{ 0xc6, "Request data truncated" },
	{ 0xc7, "Request data length invalid" },
	{ 0xc8, "Request data field length limit exceeded" },
	{ 0xc9, "Parameter out of range" },
	{ 0xca, "Cannot return number of requested data bytes" },
	{ 0xcb, "Requested sensor, data, or record not found" },
	{ 0xcc, "Invalid data field in request" },
	{ 0xcd, "Command illegal for specified sensor or record type" },
	{ 0xce, "Command response could not be provided" },
	{ 0xcf, "Cannot execute duplicated request" },
	{ 0xd0, "SDR Repository in update mode" },
	{ 0xd1, "Device firmeware in update mode" },
	{ 0xd2, "BMC initialization in progress" },
	{ 0xd3, "Destination unavailable" },
	{ 0xd4, "Insufficient privilege level" },
	{ 0xd5, "Command not supported in present state" },
	{ 0xd6, "Cannot execute command, command disabled" },
	{ 0xff, "Unspecified error" },
	{ 0x00, NULL }
};

const struct valstr ipmi_chassis_power_control_vals[] = {
	{ IPMI_CHASSIS_CTL_POWER_DOWN,   "Down/Off" },
	{ IPMI_CHASSIS_CTL_POWER_UP,     "Up/On" },
	{ IPMI_CHASSIS_CTL_POWER_CYCLE,  "Cycle" },
	{ IPMI_CHASSIS_CTL_HARD_RESET,   "Reset" },
	{ IPMI_CHASSIS_CTL_PULSE_DIAG,   "Diag" },
	{ IPMI_CHASSIS_CTL_ACPI_SOFT,    "Soft" },
	{ 0x00, NULL },
};

const struct valstr ipmi_auth_algorithms[] = {
	{ IPMI_AUTH_RAKP_NONE,      "none"      },
	{ IPMI_AUTH_RAKP_HMAC_SHA1, "hmac_sha1" },
	{ IPMI_AUTH_RAKP_HMAC_MD5,  "hmac_md5"  },
#ifdef HAVE_CRYPTO_SHA256
	{ IPMI_AUTH_RAKP_HMAC_SHA256, "hmac_sha256" },
#endif /* HAVE_CRYPTO_SHA256 */
	{ 0x00, NULL }
};

const struct valstr ipmi_integrity_algorithms[] = {
	{ IPMI_INTEGRITY_NONE,         "none" },
	{ IPMI_INTEGRITY_HMAC_SHA1_96, "hmac_sha1_96" },
	{ IPMI_INTEGRITY_HMAC_MD5_128, "hmac_md5_128" },
	{ IPMI_INTEGRITY_MD5_128 ,     "md5_128"      },
#ifdef HAVE_CRYPTO_SHA256
	{ IPMI_INTEGRITY_HMAC_SHA256_128, "sha256_128" },
#endif /* HAVE_CRYPTO_SHA256 */
	{ 0x00, NULL }
};

const struct valstr ipmi_encryption_algorithms[] = {
	{ IPMI_CRYPT_NONE,        "none"        },
	{ IPMI_CRYPT_AES_CBC_128, "aes_cbc_128" },
	{ IPMI_CRYPT_XRC4_128,    "xrc4_128"    },
	{ IPMI_CRYPT_XRC4_40,     "xrc4_40"     },
	{ 0x00, NULL }
};

const struct valstr ipmi_user_enable_status_vals[] = {
	{ 0x00, "unknown" },
	{ 0x40, "enabled" },
	{ 0x80, "disabled" },
	{ 0xC0, "reserved" },
	{ 0xFF, NULL },
};

const struct valstr picmg_frucontrol_vals[] = {
	{ 0, "Cold Reset" },
	{ 1, "Warm Reset"  },
	{ 2, "Graceful Reboot" },
	{ 3, "Issue Diagnostic Interrupt" },
	{ 4, "Quiesce" },
	{ 5, NULL },
};

const struct valstr picmg_clk_family_vals[] = {
	{ 0x00, "Unspecified" },
	{ 0x01, "SONET/SDH/PDH" },
	{ 0x02, "Reserved for PCI Express" },
	{ 0x03, "Reserved" }, /* from 03h to C8h */
	{ 0xC9, "Vendor defined clock family" }, /* from C9h to FFh */
	{ 0x00, NULL },
};

const struct oemvalstr picmg_clk_accuracy_vals[] = {
	{ 0x01, 10, "PRS" },
	{ 0x01, 20, "STU" },
	{ 0x01, 30, "ST2" },
	{ 0x01, 40, "TNC" },
	{ 0x01, 50, "ST3E" },
	{ 0x01, 60, "ST3" },
	{ 0x01, 70, "SMC" },
	{ 0x01, 80, "ST4" },
	{ 0x01, 90, "DUS" },
   { 0x02, 0xE0, "PCI Express Generation 2" },
   { 0x02, 0xF0, "PCI Express Generation 1" },
   { 0xffffff, 0x00,  NULL }
};

const struct oemvalstr picmg_clk_resource_vals[] = {
	{ 0x0, 0, "On-Carrier Device 0" },
	{ 0x0, 1, "On-Carrier Device 1" },
	{ 0x1, 1, "AMC Site 1 - A1" },
	{ 0x1, 2, "AMC Site 1 - A2" },
	{ 0x1, 3, "AMC Site 1 - A3" },
	{ 0x1, 4, "AMC Site 1 - A4" },
	{ 0x1, 5, "AMC Site 1 - B1" },
	{ 0x1, 6, "AMC Site 1 - B2" },
	{ 0x1, 7, "AMC Site 1 - B3" },
	{ 0x1, 8, "AMC Site 1 - B4" },
   { 0x2, 0, "ATCA Backplane" },
   { 0xffffff, 0x00,  NULL }
};

const struct oemvalstr picmg_clk_id_vals[] = {
	{ 0x0, 0, "Clock 0" },
	{ 0x0, 1, "Clock 1" },
	{ 0x0, 2, "Clock 2" },
	{ 0x0, 3, "Clock 3" },
	{ 0x0, 4, "Clock 4" },
	{ 0x0, 5, "Clock 5" },
	{ 0x0, 6, "Clock 6" },
	{ 0x0, 7, "Clock 7" },
	{ 0x0, 8, "Clock 8" },
	{ 0x0, 9, "Clock 9" },
	{ 0x0, 10, "Clock 10" },
	{ 0x0, 11, "Clock 11" },
	{ 0x0, 12, "Clock 12" },
	{ 0x0, 13, "Clock 13" },
	{ 0x0, 14, "Clock 14" },
	{ 0x0, 15, "Clock 15" },
	{ 0x1, 1, "TCLKA" },
	{ 0x1, 2, "TCLKB" },
	{ 0x1, 3, "TCLKC" },
	{ 0x1, 4, "TCLKD" },
	{ 0x1, 5, "FLCKA" },
   { 0x2, 1, "CLK1A" },
   { 0x2, 2, "CLK1A" },
   { 0x2, 3, "CLK1A" },
   { 0x2, 4, "CLK1A" },
   { 0x2, 5, "CLK1A" },
   { 0x2, 6, "CLK1A" },
   { 0x2, 7, "CLK1A" },
   { 0x2, 8, "CLK1A" },
   { 0x2, 9, "CLK1A" },
   { 0xffffff, 0x00,  NULL }
};

const struct valstr picmg_busres_id_vals[] = {
   { 0x0, "Metallic Test Bus pair #1" },
   { 0x1, "Metallic Test Bus pair #2" },
   { 0x2, "Synch clock group 1 (CLK1)" },
   { 0x3, "Synch clock group 2 (CLK2)" },
   { 0x4, "Synch clock group 3 (CLK3)" },
	{ 0x5, NULL }
};
const struct valstr picmg_busres_board_cmd_vals[] = {
  { 0x0, "Query" },
  { 0x1, "Release" },
  { 0x2, "Force" },
  { 0x3, "Bus Free" },
  { 0x4, NULL }
};

const struct valstr picmg_busres_shmc_cmd_vals[] = {
  { 0x0, "Request" },
  { 0x1, "Relinquish" },
  { 0x2, "Notify" },
  { 0x3, NULL }
};

const struct oemvalstr picmg_busres_board_status_vals[] = {
 { 0x0, 0x0, "In control" },
 { 0x0, 0x1, "No control" },
 { 0x1, 0x0, "Ack" },
 { 0x1, 0x1, "Refused" },
 { 0x1, 0x2, "No control" },
 { 0x2, 0x0, "Ack" },
 { 0x2, 0x1, "No control" },
 { 0x3, 0x0, "Accept" },
 { 0x3, 0x1, "Not Needed" },
 { 0xffffff, 0x00,  NULL }
};

const struct oemvalstr picmg_busres_shmc_status_vals[] = {
 { 0x0, 0x0, "Grant" },
 { 0x0, 0x1, "Busy" },
 { 0x0, 0x2, "Defer" },
 { 0x0, 0x3, "Deny" },

 { 0x1, 0x0, "Ack" },
 { 0x1, 0x1, "Error" },

 { 0x2, 0x0, "Ack" },
 { 0x2, 0x1, "Error" },
 { 0x2, 0x2, "Deny" },

 { 0xffffff, 0x00,  NULL }
};
