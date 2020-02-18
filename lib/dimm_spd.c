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

#include <ipmitool/ipmi.h>
#include <ipmitool/log.h>
#include <ipmitool/helper.h>
#include <ipmitool/ipmi_intf.h>
#include <ipmitool/ipmi_fru.h>

#include <stdlib.h>
#include <string.h>

extern int verbose;

/*
 * Also, see ipmi_fru.c.
 *
 * Apparently some systems have problems with FRU access greater than 16 bytes
 * at a time, even when using byte (not word) access.	 In order to ensure we
 * work with the widest variety of hardware request size is capped at 16 bytes.
 * Since this may result in slowdowns on some systems with lots of FRU data you
 * can change this define to enable larger (up to 32 bytes at a time) access.
 */
#define FRU_DATA_RQST_SIZE 16;

const struct valstr spd_memtype_vals[] = {
	{ 0x01, "STD FPM DRAM" },
	{ 0x02, "EDO" },
	{ 0x04, "SDRAM" },
	{ 0x05, "ROM" },
	{ 0x06, "DDR SGRAM" },
	{ 0x07, "DDR SDRAM" },
	{ 0x08, "DDR2 SDRAM" },
	{ 0x09, "DDR2 SDRAM FB-DIMM" },
	{ 0x0A, "DDR2 SDRAM FB-DIMM Probe" },
	{ 0x0B, "DDR3 SDRAM" },
	{ 0x0C, "DDR4 SDRAM" },
	{ 0x00, NULL },
};

const struct valstr ddr3_density_vals[] =
{
	{ 0, "256 Mb" },
	{ 1, "512 Mb" },
	{ 2, "1 Gb" },
	{ 3, "2 Gb" },
	{ 4, "4 Gb" },
	{ 5, "8 Gb" },
	{ 6, "16 Gb" },
	{ 0x00, NULL },
};

const struct valstr ddr3_banks_vals[] =
{
	{ 0, "3 (8 Banks)" },
	{ 1, "4 (16 Banks)" },
	{ 2, "5 (32 Banks)" },
	{ 3, "6 (64 Banks)" },
	{ 0x00, NULL },
};


#define ddr4_ecc_vals ddr3_ecc_vals
const struct valstr ddr3_ecc_vals[] =
{
	{ 0, "0 bits" },
	{ 1, "8 bits" },
	{ 0x00, NULL },
};

const struct valstr ddr4_density_vals[] =
{
	{ 0, "256 Mb" },
	{ 1, "512 Mb" },
	{ 2, "1 Gb" },
	{ 3, "2 Gb" },
	{ 4, "4 Gb" },
	{ 5, "8 Gb" },
	{ 6, "16 Gb" },
	{ 7, "32 Gb" },
	{ 0x00, NULL },
};

const struct valstr ddr4_banks_vals[] =
{
	{ 0, "2 (4 Banks)" },
	{ 1, "3 (8 Banks)" },
	{ 0x00, NULL },
};

const struct valstr ddr4_bank_groups[] =
{
	{ 0, "0 (no Bank Groups)" },
	{ 1, "1 (2 Bank Groups)" },
	{ 2, "2 (4 Bank Groups)" },
	{ 0x00, NULL },
};

const struct valstr ddr4_package_type[] =
{
	{ 0, "Monolithic DRAM Device" },
	{ 1, "Non-Monolithic Device" },
	{ 0x00, NULL },
};

const struct valstr ddr4_technology_type[] =
{
	{ 0, "Extended module type, see byte 15" },
	{ 1, "RDIMM" },
	{ 2, "UDIMM" },
	{ 3, "SO-DIMM" },
	{ 4, "LRDIMM" },
	{ 5, "Mini-RDIMM" },
	{ 6, "Mini-UDIMM" },
	{ 7, "7 - Reserved" },
	{ 8, "72b-SO-RDIMM" },
	{ 9, "72b-SO-UDIMM" },
	{ 10, "10 - Reserved" },
	{ 11, "11 - Reserved" },
	{ 12, "16b-SO-DIMM" },
	{ 13, "32b-SO-DIMM" },
	{ 14, "14 - Reserved" },
	{ 15, "No base memory present" },
	{ 0x00, NULL },
};

const struct valstr spd_config_vals[] = {
	{ 0x00, "None" },
	{ 0x01, "Parity" },
	{ 0x02, "ECC" },
	{ 0x04, "Addr Cmd Parity" },
	{ 0x00, NULL },
};

const struct valstr spd_voltage_vals[] = {
	{ 0x00, "5.0V TTL" },
	{ 0x01, "LVTTL" },
	{ 0x02, "HSTL 1.5V" },
	{ 0x03, "SSTL 3.3V" },
	{ 0x04, "SSTL 2.5V" },
	{ 0x05, "SSTL 1.8V" },
	{ 0x00, NULL },
};

/*
 * JEDEC Standard Manufacturers Identification Code
 * publication JEP106N, December 2003
 */

const struct valstr jedec_id1_vals[] = {
	{ 0x01, "AMD" },
	{ 0x02, "AMI" },
	{ 0x83, "Fairchild" },
	{ 0x04, "Fujitsu" },
	{ 0x85, "GTE" },
	{ 0x86, "Harris" },
	{ 0x07, "Hitachi" },
	{ 0x08, "Inmos" },
	{ 0x89, "Intel" },
	{ 0x8A, "I.T.T." },
	{ 0x0B, "Intersil" },
	{ 0x8C, "Monolithic Memories" },
	{ 0x0D, "Mostek" },
	{ 0x0E, "Freescale (Motorola)" },
	{ 0x8F, "National" },
	{ 0x10, "NEC" },
	{ 0x91, "RCA" },
	{ 0x92, "Raytheon" },
	{ 0x13, "Conexant (Rockwell)" },
	{ 0x94, "Seeq" },
	{ 0x15, "NXP (Philips)" },
	{ 0x16, "Synertek" },
	{ 0x97, "Texas Instruments" },
	{ 0x98, "Toshiba" },
	{ 0x19, "Xicor" },
	{ 0x1A, "Zilog" },
	{ 0x9B, "Eurotechnique" },
	{ 0x1C, "Mitsubishi" },
	{ 0x9D, "Lucent (AT&T)" },
	{ 0x9E, "Exel" },
	{ 0x1F, "Atmel" },
	{ 0x20, "STMicroelectronics" },
	{ 0xA1, "Lattice Semi." },
	{ 0xA2, "NCR" },
	{ 0x23, "Wafer Scale Integration" },
	{ 0xA4, "IBM" },
	{ 0x25, "Tristar" },
	{ 0x26, "Visic" },
	{ 0xA7, "Intl. CMOS Technology" },
	{ 0xA8, "SSSI" },
	{ 0x29, "MicrochipTechnology" },
	{ 0x2A, "Ricoh Ltd." },
	{ 0xAB, "VLSI" },
	{ 0x2C, "Micron Technology" },
	{ 0xAD, "SK Hynix" },
	{ 0xAE, "OKI Semiconductor" },
	{ 0x2F, "ACTEL" },
	{ 0xB0, "Sharp" },
	{ 0x31, "Catalyst" },
	{ 0x32, "Panasonic" },
	{ 0xB3, "IDT" },
	{ 0x34, "Cypress" },
	{ 0xB5, "DEC" },
	{ 0xB6, "LSI Logic" },
	{ 0x37, "Zarlink (Plessey)" },
	{ 0x38, "UTMC" },
	{ 0xB9, "Thinking Machine" },
	{ 0xBA, "Thomson CSF" },
	{ 0x3B, "Integrated CMOS (Vertex)" },
	{ 0xBC, "Honeywell" },
	{ 0x3D, "Tektronix" },
	{ 0x3E, "Oracle Corporation" },
	{ 0xBF, "Silicon Storage Technology" },
	{ 0x40, "ProMos/Mosel Vitelic" },
	{ 0xC1, "Infineon (Siemens)" },
	{ 0xC2, "Macronix" },
	{ 0x43, "Xerox" },
	{ 0xC4, "Plus Logic" },
	{ 0x45, "SanDisk Corporation" },
	{ 0x46, "Elan Circuit Tech." },
	{ 0xC7, "European Silicon Str." },
	{ 0xC8, "Apple Computer" },
	{ 0x49, "Xilinx" },
	{ 0x4A, "Compaq" },
	{ 0xCB, "Protocol Engines" },
	{ 0x4C, "SCI" },
	{ 0xCD, "Seiko Instruments" },
	{ 0xCE, "Samsung" },
	{ 0x4F, "I3 Design System" },
	{ 0xD0, "Klic" },
	{ 0x51, "Crosspoint Solutions" },
	{ 0x52, "Alliance Semiconductor" },
	{ 0xD3, "Tandem" },
	{ 0x54, "Hewlett-Packard" },
	{ 0xD5, "Integrated Silicon Solutions" },
	{ 0xD6, "Brooktree" },
	{ 0x57, "New Media" },
	{ 0x58, "MHS Electronic" },
	{ 0xD9, "Performance Semi." },
	{ 0xDA, "Winbond Electronic" },
	{ 0x5B, "Kawasaki Steel" },
	{ 0xDC, "Bright Micro" },
	{ 0x5D, "TECMAR" },
	{ 0x5E, "Exar" },
	{ 0xDF, "PCMCIA" },
	{ 0xE0, "LG Semi (Goldstar)" },
	{ 0x61, "Northern Telecom" },
	{ 0x62, "Sanyo" },
	{ 0xE3, "Array Microsystems" },
	{ 0x64, "Crystal Semiconductor" },
	{ 0xE5, "Analog Devices" },
	{ 0xE6, "PMC-Sierra" },
	{ 0x67, "Asparix" },
	{ 0x68, "Convex Computer" },
	{ 0xE9, "Quality Semiconductor" },
	{ 0xEA, "Nimbus Technology" },
	{ 0x6B, "Transwitch" },
	{ 0xEC, "Micronas (ITT Intermetall)" },
	{ 0x6D, "Cannon" },
	{ 0x6E, "Altera" },
	{ 0xEF, "NEXCOM" },
	{ 0x70, "Qualcomm" },
	{ 0xF1, "Sony" },
	{ 0xF2, "Cray Research" },
	{ 0x73, "AMS(Austria Micro)" },
	{ 0xF4, "Vitesse" },
	{ 0x75, "Aster Electronics" },
	{ 0x76, "Bay Networks (Synoptic)" },
	{ 0xF7, "Zentrum/ZMD" },
	{ 0xF8, "TRW" },
	{ 0x79, "Thesys" },
	{ 0x7A, "Solbourne Computer" },
	{ 0xFB, "Allied-Signal" },
	{ 0x7C, "Dialog Semiconductor" },
	{ 0xFD, "Media Vision" },
	{ 0xFE, "Numonyx Corporation" },
	{ 0x00, NULL },
};

const struct valstr jedec_id2_vals[] = {
	{ 0x01, "Cirrus Logic" },
	{ 0x02, "National Instruments" },
	{ 0x83, "ILC Data Device" },
	{ 0x04, "Alcatel Mietec" },
	{ 0x85, "Micro Linear" },
	{ 0x86, "Univ. of NC" },
	{ 0x07, "JTAG Technologies" },
	{ 0x08, "BAE Systems (Loral)" },
	{ 0x89, "Nchip" },
	{ 0x8A, "Galileo Tech" },
	{ 0x0B, "Bestlink Systems" },
	{ 0x8C, "Graychip" },
	{ 0x0D, "GENNUM" },
	{ 0x0E, "VideoLogic" },
	{ 0x8F, "Robert Bosch" },
	{ 0x10, "Chip Express" },
	{ 0x91, "DATARAM" },
	{ 0x92, "United Microelectronics Corp." },
	{ 0x13, "TCSI" },
	{ 0x94, "Smart Modular" },
	{ 0x15, "Hughes Aircraft" },
	{ 0x16, "Lanstar Semiconductor" },
	{ 0x97, "Qlogic" },
	{ 0x98, "Kingston" },
	{ 0x19, "Music Semi" },
	{ 0x1A, "Ericsson Components" },
	{ 0x9B, "SpaSE" },
	{ 0x1C, "Eon Silicon Devices" },
	{ 0x9D, "Integrated Silicon Solution (ISSI)" },
	{ 0x9E, "DoD" },
	{ 0x1F, "Integ. Memories Tech." },
	{ 0x20, "Corollary Inc." },
	{ 0xA1, "Dallas Semiconductor" },
	{ 0xA2, "Omnivision" },
	{ 0x23, "EIV(Switzerland)" },
	{ 0xA4, "Novatel Wireless" },
	{ 0x25, "Zarlink (Mitel)" },
	{ 0x26, "Clearpoint" },
	{ 0xA7, "Cabletron" },
	{ 0xA8, "STEC (Silicon Tech)" },
	{ 0x29, "Vanguard" },
	{ 0x2A, "Hagiwara Sys-Com" },
	{ 0xAB, "Vantis" },
	{ 0x2C, "Celestica" },
	{ 0xAD, "Century" },
	{ 0xAE, "Hal Computers" },
	{ 0x2F, "Rohm Company Ltd." },
	{ 0xB0, "Juniper Networks" },
	{ 0x31, "Libit Signal Processing" },
	{ 0x32, "Mushkin Enhanced Memory" },
	{ 0xB3, "Tundra Semiconductor" },
	{ 0x34, "Adaptec Inc." },
	{ 0xB5, "LightSpeed Semi." },
	{ 0xB6, "ZSP Corp." },
	{ 0x37, "AMIC Technology" },
	{ 0x38, "Adobe Systems" },
	{ 0xB9, "Dynachip" },
	{ 0xBA, "PNY Technologies, Inc." },
	{ 0x3B, "Newport Digital" },
	{ 0xBC, "MMC Networks" },
	{ 0x3D, "T Square" },
	{ 0x3E, "Seiko Epson" },
	{ 0xBF, "Broadcom" },
	{ 0x40, "Viking Components" },
	{ 0xC1, "V3 Semiconductor" },
	{ 0xC2, "Flextronics (Orbit Semiconductor)" },
	{ 0x43, "Suwa Electronics" },
	{ 0xC4, "Transmeta" },
	{ 0x45, "Micron CMS" },
	{ 0x46, "American Computer & Digital Components Inc." },
	{ 0xC7, "Enhance 3000 Inc." },
	{ 0xC8, "Tower Semiconductor" },
	{ 0x49, "CPU Design" },
	{ 0x4A, "Price Point" },
	{ 0xCB, "Maxim Integrated Product" },
	{ 0x4C, "Tellabs" },
	{ 0xCD, "Centaur Technology" },
	{ 0xCE, "Unigen Corporation" },
	{ 0x4F, "Transcend Information" },
	{ 0xD0, "Memory Card Technology" },
	{ 0x51, "CKD Corporation Ltd." },
	{ 0x52, "Capital Instruments, Inc." },
	{ 0xD3, "Aica Kogyo, Ltd." },
	{ 0x54, "Linvex Technology" },
	{ 0xD5, "MSC Vertriebs GmbH" },
	{ 0xD6, "AKM Company, Ltd." },
	{ 0x57, "Dynamem, Inc." },
	{ 0x58, "NERA ASA" },
	{ 0xD9, "GSI Technology" },
	{ 0xDA, "Dane-Elec (C Memory)" },
	{ 0x5B, "Acorn Computers" },
	{ 0xDC, "Lara Technology" },
	{ 0x5D, "Oak Technology, Inc." },
	{ 0x5E, "Itec Memory" },
	{ 0xDF, "Tanisys Technology" },
	{ 0xE0, "Truevision" },
	{ 0x61, "Wintec Industries" },
	{ 0x62, "Super PC Memory" },
	{ 0xE3, "MGV Memory" },
	{ 0x64, "Galvantech" },
	{ 0xE5, "Gadzoox Networks" },
	{ 0xE6, "Multi Dimensional Cons." },
	{ 0x67, "GateField" },
	{ 0x68, "Integrated Memory System" },
	{ 0xE9, "Triscend" },
	{ 0xEA, "XaQti" },
	{ 0x6B, "Goldenram" },
	{ 0xEC, "Clear Logic" },
	{ 0x6D, "Cimaron Communications" },
	{ 0x6E, "Nippon Steel Semi. Corp." },
	{ 0xEF, "Advantage Memory" },
	{ 0x70, "AMCC" },
	{ 0xF1, "LeCroy" },
	{ 0xF2, "Yamaha Corporation" },
	{ 0x73, "Digital Microwave" },
	{ 0xF4, "NetLogic Microsystems" },
	{ 0x75, "MIMOS Semiconductor" },
	{ 0x76, "Advanced Fibre" },
	{ 0xF7, "BF Goodrich Data." },
	{ 0xF8, "Epigram" },
	{ 0x79, "Acbel Polytech Inc." },
	{ 0x7A, "Apacer Technology" },
	{ 0xFB, "Admor Memory" },
	{ 0x7C, "FOXCONN" },
	{ 0xFD, "Quadratics Superconductor" },
	{ 0xFE, "3COM" },
	{ 0x00, NULL },
};

const struct valstr jedec_id3_vals[] = {
	{ 0x01, "Camintonn Corporation" },
	{ 0x02, "ISOA Incorporated" },
	{ 0x83, "Agate Semiconductor" },
	{ 0x04, "ADMtek Incorporated" },
	{ 0x85, "HYPERTEC" },
	{ 0x86, "Adhoc Technologies" },
	{ 0x07, "MOSAID Technologies" },
	{ 0x08, "Ardent Technologies" },
	{ 0x89, "Switchcore" },
	{ 0x8A, "Cisco Systems, Inc." },
	{ 0x0B, "Allayer Technologies" },
	{ 0x8C, "WorkX AG (Wichman)" },
	{ 0x0D, "Oasis Semiconductor" },
	{ 0x0E, "Novanet Semiconductor" },
	{ 0x8F, "E-M Solutions" },
	{ 0x10, "Power General" },
	{ 0x91, "Advanced Hardware Arch." },
	{ 0x92, "Inova Semiconductors GmbH" },
	{ 0x13, "Telocity" },
	{ 0x94, "Delkin Devices" },
	{ 0x15, "Symagery Microsystems" },
	{ 0x16, "C-Port Corporation" },
	{ 0x97, "SiberCore Technologies" },
	{ 0x98, "Southland Microsystems" },
	{ 0x19, "Malleable Technologies" },
	{ 0x1A, "Kendin Communications" },
	{ 0x9B, "Great Technology Microcomputer" },
	{ 0x1C, "Sanmina Corporation" },
	{ 0x9D, "HADCO Corporation" },
	{ 0x9E, "Corsair" },
	{ 0x1F, "Actrans System Inc." },
	{ 0x20, "ALPHA Technologies" },
	{ 0xA1, "Silicon Laboratories, Inc. (Cygnal)" },
	{ 0xA2, "Artesyn Technologies" },
	{ 0x23, "Align Manufacturing" },
	{ 0xA4, "Peregrine Semiconductor" },
	{ 0x25, "Chameleon Systems" },
	{ 0x26, "Aplus Flash Technology" },
	{ 0xA7, "MIPS Technologies" },
	{ 0xA8, "Chrysalis ITS" },
	{ 0x29, "ADTEC Corporation" },
	{ 0x2A, "Kentron Technologies" },
	{ 0xAB, "Win Technologies" },
	{ 0x2C, "Tezzaron Semiconductor" },
	{ 0xAD, "Extreme Packet Devices" },
	{ 0xAE, "RF Micro Devices" },
	{ 0x2F, "Siemens AG" },
	{ 0xB0, "Sarnoff Corporation" },
	{ 0x31, "Itautec SA" },
	{ 0x32, "Radiata Inc." },
	{ 0xB3, "Benchmark Elect. (AVEX)" },
	{ 0x34, "Legend" },
	{ 0xB5, "SpecTek Incorporated" },
	{ 0xB6, "Hi/fn" },
	{ 0x37, "Enikia Incorporated" },
	{ 0x38, "SwitchOn Networks" },
	{ 0xB9, "AANetcom Incorporated" },
	{ 0xBA, "Micro Memory Bank" },
	{ 0x3B, "ESS Technology" },
	{ 0xBC, "Virata Corporation" },
	{ 0x3D, "Excess Bandwidth" },
	{ 0x3E, "West Bay Semiconductor" },
	{ 0xBF, "DSP Group" },
	{ 0x40, "Newport Communications" },
	{ 0xC1, "Chip2Chip Incorporated" },
	{ 0xC2, "Phobos Corporation" },
	{ 0x43, "Intellitech Corporation" },
	{ 0xC4, "Nordic VLSI ASA" },
	{ 0x45, "Ishoni Networks" },
	{ 0x46, "Silicon Spice" },
	{ 0xC7, "Alchemy Semiconductor" },
	{ 0xC8, "Agilent Technologies" },
	{ 0x49, "Centillium Communications" },
	{ 0x4A, "W.L. Gore" },
	{ 0xCB, "HanBit Electronics" },
	{ 0x4C, "GlobeSpan" },
	{ 0xCD, "Element 14" },
	{ 0xCE, "Pycon" },
	{ 0x4F, "Saifun Semiconductors" },
	{ 0xD0, "Sibyte, Incorporated" },
	{ 0x51, "MetaLink Technologies" },
	{ 0x52, "Feiya Technology" },
	{ 0xD3, "I & C Technology" },
	{ 0x54, "Shikatronics" },
	{ 0xD5, "Elektrobit" },
	{ 0xD6, "Megic" },
	{ 0x57, "Com-Tier" },
	{ 0x58, "Malaysia Micro Solutions" },
	{ 0xD9, "Hyperchip" },
	{ 0xDA, "Gemstone Communications" },
	{ 0x5B, "Anadigm (Anadyne)" },
	{ 0xDC, "3ParData" },
	{ 0x5D, "Mellanox Technologies" },
	{ 0x5E, "Tenx Technologies" },
	{ 0xDF, "Helix AG" },
	{ 0xE0, "Domosys" },
	{ 0x61, "Skyup Technology" },
	{ 0x62, "HiNT Corporation" },
	{ 0xE3, "Chiaro" },
	{ 0x64, "MDT Technologies GmbH" },
	{ 0xE5, "Exbit Technology A/S" },
	{ 0xE6, "Integrated Technology Express" },
	{ 0x67, "AVED Memory" },
	{ 0x68, "Legerity" },
	{ 0xE9, "Jasmine Networks" },
	{ 0xEA, "Caspian Networks" },
	{ 0x6B, "nCUBE" },
	{ 0xEC, "Silicon Access Networks" },
	{ 0x6D, "FDK Corporation" },
	{ 0x6E, "High Bandwidth Access" },
	{ 0xEF, "MultiLink Technology" },
	{ 0x70, "BRECIS" },
	{ 0xF1, "World Wide Packets" },
	{ 0xF2, "APW" },
	{ 0x73, "Chicory Systems" },
	{ 0xF4, "Xstream Logic" },
	{ 0x75, "Fast-Chip" },
	{ 0x76, "Zucotto Wireless" },
	{ 0xF7, "Realchip" },
	{ 0xF8, "Galaxy Power" },
	{ 0x79, "eSilicon" },
	{ 0x7A, "Morphics Technology" },
	{ 0xFB, "Accelerant Networks" },
	{ 0x7C, "Silicon Wave" },
	{ 0xFD, "SandCraft" },
	{ 0xFE, "Elpida" },
	{ 0x00, NULL },
};

const struct valstr jedec_id4_vals[] = {
	{ 0x01, "Solectron" },
	{ 0x02, "Optosys Technologies" },
	{ 0x83, "Buffalo (Formerly Melco)" },
	{ 0x04, "TriMedia Technologies" },
	{ 0x85, "Cyan Technologies" },
	{ 0x86, "Global Locate" },
	{ 0x07, "Optillion" },
	{ 0x08, "Terago Communications" },
	{ 0x89, "Ikanos Communications" },
	{ 0x8A, "Princeton Technology" },
	{ 0x0B, "Nanya Technology" },
	{ 0x8C, "Elite Flash Storage" },
	{ 0x0D, "Mysticom" },
	{ 0x0E, "LightSand Communications" },
	{ 0x8F, "ATI Technologies" },
	{ 0x10, "Agere Systems" },
	{ 0x91, "NeoMagic" },
	{ 0x92, "AuroraNetics" },
	{ 0x13, "Golden Empire" },
	{ 0x94, "Mushkin" },
	{ 0x15, "Tioga Technologies" },
	{ 0x16, "Netlist" },
	{ 0x97, "TeraLogic" },
	{ 0x98, "Cicada Semiconductor" },
	{ 0x19, "Centon Electronics" },
	{ 0x1A, "Tyco Electronics" },
	{ 0x9B, "Magis Works" },
	{ 0x1C, "Zettacom" },
	{ 0x9D, "Cogency Semiconductor" },
	{ 0x9E, "Chipcon AS" },
	{ 0x1F, "Aspex Technology" },
	{ 0x20, "F5 Networks" },
	{ 0xA1, "Programmable Silicon Solutions" },
	{ 0xA2, "ChipWrights" },
	{ 0x23, "Acorn Networks" },
	{ 0xA4, "Quicklogic" },
	{ 0x25, "Kingmax Semiconductor" },
	{ 0x26, "BOPS" },
	{ 0xA7, "Flasys" },
	{ 0xA8, "BitBlitz Communications" },
	{ 0x29, "eMemory Technology" },
	{ 0x2A, "Procket Networks" },
	{ 0xAB, "Purple Ray" },
	{ 0x2C, "Trebia Networks" },
	{ 0xAD, "Delta Electronics" },
	{ 0xAE, "Onex Communications" },
	{ 0x2F, "Ample Communications" },
	{ 0xB0, "Memory Experts Intl" },
	{ 0x31, "Astute Networks" },
	{ 0x32, "Azanda Network Devices" },
	{ 0xB3, "Dibcom" },
	{ 0x34, "Tekmos" },
	{ 0xB5, "API NetWorks" },
	{ 0xB6, "Bay Microsystems" },
	{ 0x37, "Firecron Ltd" },
	{ 0x38, "Resonext Communications" },
	{ 0xB9, "Tachys Technologies" },
	{ 0xBA, "Equator Technology" },
	{ 0x3B, "Concept Computer" },
	{ 0xBC, "SILCOM" },
	{ 0x3D, "3Dlabs" },
	{ 0x3E, "c’t Magazine" },
	{ 0xBF, "Sanera Systems" },
	{ 0x40, "Silicon Packets" },
	{ 0xC1, "Viasystems Group" },
	{ 0xC2, "Simtek" },
	{ 0x43, "Semicon Devices Singapore" },
	{ 0xC4, "Satron Handelsges" },
	{ 0x45, "Improv Systems" },
	{ 0x46, "INDUSYS GmbH" },
	{ 0xC7, "Corrent" },
	{ 0xC8, "Infrant Technologies" },
	{ 0x49, "Ritek Corp" },
	{ 0x4A, "empowerTel Networks" },
	{ 0xCB, "Hypertec" },
	{ 0x4C, "Cavium Networks" },
	{ 0xCD, "PLX Technology" },
	{ 0xCE, "Massana Design" },
	{ 0x4F, "Intrinsity" },
	{ 0xD0, "Valence Semiconductor" },
	{ 0x51, "Terawave Communications" },
	{ 0x52, "IceFyre Semiconductor" },
	{ 0xD3, "Primarion" },
	{ 0x54, "Picochip Designs Ltd" },
	{ 0xD5, "Silverback Systems" },
	{ 0xD6, "Jade Star Technologies" },
	{ 0x57, "Pijnenburg Securealink" },
	{ 0x58, "takeMS - Ultron AG" },
	{ 0xD9, "Cambridge Silicon Radio" },
	{ 0xDA, "Swissbit" },
	{ 0x5B, "Nazomi Communications" },
	{ 0xDC, "eWave System" },
	{ 0x5D, "Rockwell Collins" },
	{ 0x5E, "Picocel Co. Ltd. (Paion)" },
	{ 0xDF, "Alphamosaic Ltd" },
	{ 0xE0, "Sandburst" },
	{ 0x61, "SiCon Video" },
	{ 0x62, "NanoAmp Solutions" },
	{ 0xE3, "Ericsson Technology" },
	{ 0x64, "PrairieComm" },
	{ 0xE5, "Mitac International" },
	{ 0xE6, "Layer N Networks" },
	{ 0x67, "MtekVision (Atsana)" },
	{ 0x68, "Allegro Networks" },
	{ 0xE9, "Marvell Semiconductors" },
	{ 0xEA, "Netergy Microelectronic" },
	{ 0x6B, "NVIDIA" },
	{ 0xEC, "Internet Machines" },
	{ 0x6D, "Memorysolution GmbH" },
	{ 0x6E, "Litchfield Communication" },
	{ 0xEF, "Accton Technology" },
	{ 0x70, "Teradiant Networks" },
	{ 0xF1, "Scaleo Chip" },
	{ 0xF2, "Cortina Systems" },
	{ 0x73, "RAM Components" },
	{ 0xF4, "Raqia Networks" },
	{ 0x75, "ClearSpeed" },
	{ 0x76, "Matsushita Battery" },
	{ 0xF7, "Xelerated" },
	{ 0xF8, "SimpleTech" },
	{ 0x79, "Utron Technology" },
	{ 0x7A, "Astec International" },
	{ 0xFB, "AVM gmbH" },
	{ 0x7C, "Redux Communications" },
	{ 0xFD, "Dot Hill Systems" },
	{ 0xFE, "TeraChip" },
	{ 0x00, NULL },
};

const struct valstr jedec_id5_vals[] = {
	{ 0x01, "T-RAM Incorporated" },
	{ 0x02, "Innovics Wireless" },
	{ 0x83, "Teknovus" },
	{ 0x04, "KeyEye Communications" },
	{ 0x85, "Runcom Technologies" },
	{ 0x86, "RedSwitch" },
	{ 0x07, "Dotcast" },
	{ 0x08, "Silicon Mountain Memory" },
	{ 0x89, "Signia Technologies" },
	{ 0x8A, "Pixim" },
	{ 0x0B, "Galazar Networks" },
	{ 0x8C, "White Electronic Designs" },
	{ 0x0D, "Patriot Scientific" },
	{ 0x0E, "Neoaxiom Corporation" },
	{ 0x8F, "3Y Power Technology" },
	{ 0x10, "Scaleo Chip" },
	{ 0x91, "Potentia Power Systems" },
	{ 0x92, "C-guys Incorporated" },
	{ 0x13, "Digital Communications Technology Incorporated" },
	{ 0x94, "Silicon-Based Technology" },
	{ 0x15, "Fulcrum Microsystems" },
	{ 0x16, "Positivo Informatica Ltd" },
	{ 0x97, "XIOtech Corporation" },
	{ 0x98, "PortalPlayer" },
	{ 0x19, "Zhiying Software" },
	{ 0x1A, "ParkerVision, Inc." },
	{ 0x9B, "Phonex Broadband" },
	{ 0x1C, "Skyworks Solutions" },
	{ 0x9D, "Entropic Communications" },
	{ 0x9E, "I’M Intelligent Memory Ltd." },
	{ 0x1F, "Zensys A/S" },
	{ 0x20, "Legend Silicon Corp." },
	{ 0xA1, "Sci-worx GmbH" },
	{ 0xA2, "SMSC (Standard Microsystems)" },
	{ 0x23, "Renesas Electronics" },
	{ 0xA4, "Raza Microelectronics" },
	{ 0x25, "Phyworks" },
	{ 0x26, "MediaTek" },
	{ 0xA7, "Non-cents Productions" },
	{ 0xA8, "US Modular" },
	{ 0x29, "Wintegra Ltd." },
	{ 0x2A, "Mathstar" },
	{ 0xAB, "StarCore" },
	{ 0x2C, "Oplus Technologies" },
	{ 0xAD, "Mindspeed" },
	{ 0xAE, "Just Young Computer" },
	{ 0x2F, "Radia Communications" },
	{ 0xB0, "OCZ" },
	{ 0x31, "Emuzed" },
	{ 0x32, "LOGIC Devices" },
	{ 0xB3, "Inphi Corporation" },
	{ 0x34, "Quake Technologies" },
	{ 0xB5, "Vixel" },
	{ 0xB6, "SolusTek" },
	{ 0x37, "Kongsberg Maritime" },
	{ 0x38, "Faraday Technology" },
	{ 0xB9, "Altium Ltd." },
	{ 0xBA, "Insyte" },
	{ 0x3B, "ARM Ltd." },
	{ 0xBC, "DigiVision" },
	{ 0x3D, "Vativ Technologies" },
	{ 0x3E, "Endicott Interconnect Technologies" },
	{ 0xBF, "Pericom" },
	{ 0x40, "Bandspeed" },
	{ 0xC1, "LeWiz Communications" },
	{ 0xC2, "CPU Technology" },
	{ 0x43, "Ramaxel Technology" },
	{ 0xC4, "DSP Group" },
	{ 0x45, "Axis Communications" },
	{ 0x46, "Legacy Electronics" },
	{ 0xC7, "Chrontel" },
	{ 0xC8, "Powerchip Semiconductor" },
	{ 0x49, "MobilEye Technologies" },
	{ 0x4A, "Excel Semiconductor" },
	{ 0xCB, "A-DATA Technology" },
	{ 0x4C, "VirtualDigm" },
	{ 0xCD, "G Skill Intl" },
	{ 0xCE, "Quanta Computer" },
	{ 0x4F, "Yield Microelectronics" },
	{ 0xD0, "Afa Technologies" },
	{ 0x51, "KINGBOX Technology Co. Ltd." },
	{ 0x52, "Ceva" },
	{ 0xD3, "iStor Networks" },
	{ 0x54, "Advance Modules" },
	{ 0xD5, "Microsoft" },
	{ 0xD6, "Open-Silicon" },
	{ 0x57, "Goal Semiconductor" },
	{ 0x58, "ARC International" },
	{ 0xD9, "Simmtec" },
	{ 0xDA, "Metanoia" },
	{ 0x5B, "Key Stream" },
	{ 0xDC, "Lowrance Electronics" },
	{ 0x5D, "Adimos" },
	{ 0x5E, "SiGe Semiconductor" },
	{ 0xDF, "Fodus Communications" },
	{ 0xE0, "Credence Systems Corp." },
	{ 0x61, "Genesis Microchip Inc." },
	{ 0x62, "Vihana, Inc." },
	{ 0xE3, "WIS Technologies" },
	{ 0x64, "GateChange Technologies" },
	{ 0xE5, "High Density Devices AS" },
	{ 0xE6, "Synopsys" },
	{ 0x67, "Gigaram" },
	{ 0x68, "Enigma Semiconductor Inc." },
	{ 0xE9, "Century Micro Inc." },
	{ 0xEA, "Icera Semiconductor" },
	{ 0x6B, "Mediaworks Integrated Systems" },
	{ 0xEC, "O’Neil Product Development" },
	{ 0x6D, "Supreme Top Technology Ltd." },
	{ 0x6E, "MicroDisplay Corporation" },
	{ 0xEF, "Team Group Inc." },
	{ 0x70, "Sinett Corporation" },
	{ 0xF1, "Toshiba Corporation" },
	{ 0xF2, "Tensilica" },
	{ 0x73, "SiRF Technology" },
	{ 0xF4, "Bacoc Inc." },
	{ 0x75, "SMaL Camera Technologies" },
	{ 0x76, "Thomson SC" },
	{ 0xF7, "Airgo Networks" },
	{ 0xF8, "Wisair Ltd." },
	{ 0x79, "SigmaTel" },
	{ 0x7A, "Arkados" },
	{ 0xFB, "Compete IT gmbH Co. KG" },
	{ 0x7C, "Eudar Technology Inc." },
	{ 0xFD, "Focus Enhancements" },
	{ 0xFE, "Xyratex" },
	{ 0x00, NULL },
};

const struct valstr jedec_id6_vals[] = {
	{ 0x01, "Specular Networks" },
	{ 0x02, "Patriot Memory (PDP Systems)" },
	{ 0x83, "U-Chip Technology Corp." },
	{ 0x04, "Silicon Optix" },
	{ 0x85, "Greenfield Networks" },
	{ 0x86, "CompuRAM GmbH" },
	{ 0x07, "Stargen, Inc." },
	{ 0x08, "NetCell Corporation" },
	{ 0x89, "Excalibrus Technologies Ltd" },
	{ 0x8A, "SCM Microsystems" },
	{ 0x0B, "Xsigo Systems, Inc." },
	{ 0x8C, "CHIPS & Systems Inc" },
	{ 0x0D, "Tier 1 Multichip Solutions" },
	{ 0x0E, "CWRL Labs" },
	{ 0x8F, "Teradici" },
	{ 0x10, "Gigaram, Inc." },
	{ 0x91, "g2 Microsystems" },
	{ 0x92, "PowerFlash Semiconductor" },
	{ 0x13, "P.A. Semi, Inc." },
	{ 0x94, "NovaTech Solutions, S.A." },
	{ 0x15, "c2 Microsystems, Inc." },
	{ 0x16, "Level5 Networks" },
	{ 0x97, "COS Memory AG" },
	{ 0x98, "Innovasic Semiconductor" },
	{ 0x19, "02IC Co. Ltd" },
	{ 0x1A, "Tabula, Inc." },
	{ 0x9B, "Crucial Technology" },
	{ 0x1C, "Chelsio Communications" },
	{ 0x9D, "Solarflare Communications" },
	{ 0x9E, "Xambala Inc." },
	{ 0x1F, "EADS Astrium" },
	{ 0x20, "Terra Semiconductor, Inc." },
	{ 0xA1, "Imaging Works, Inc." },
	{ 0xA2, "Astute Networks, Inc." },
	{ 0x23, "Tzero" },
	{ 0xA4, "Emulex" },
	{ 0x25, "Power-One" },
	{ 0x26, "Pulse~LINK Inc." },
	{ 0xA7, "Hon Hai Precision Industry" },
	{ 0xA8, "White Rock Networks Inc." },
	{ 0x29, "Telegent Systems USA, Inc." },
	{ 0x2A, "Atrua Technologies, Inc." },
	{ 0xAB, "Acbel Polytech Inc." },
	{ 0x2C, "eRide Inc." },
	{ 0xAD, "ULi Electronics Inc." },
	{ 0xAE, "Magnum Semiconductor Inc." },
	{ 0x2F, "neoOne Technology, Inc." },
	{ 0xB0, "Connex Technology, Inc." },
	{ 0x31, "Stream Processors, Inc." },
	{ 0x32, "Focus Enhancements" },
	{ 0xB3, "Telecis Wireless, Inc." },
	{ 0x34, "uNav Microelectronics" },
	{ 0xB5, "Tarari, Inc." },
	{ 0xB6, "Ambric, Inc." },
	{ 0x37, "Newport Media, Inc." },
	{ 0x38, "VMTS" },
	{ 0xB9, "Enuclia Semiconductor, Inc." },
	{ 0xBA, "Virtium Technology Inc." },
	{ 0x3B, "Solid State System Co., Ltd." },
	{ 0xBC, "Kian Tech LLC" },
	{ 0x3D, "Artimi" },
	{ 0x3E, "Power Quotient International" },
	{ 0xBF, "Avago Technologies" },
	{ 0x40, "ADTechnology" },
	{ 0xC1, "Sigma Designs" },
	{ 0xC2, "SiCortex, Inc." },
	{ 0x43, "Ventura Technology Group" },
	{ 0xC4, "eASIC" },
	{ 0x45, "M.H.S. SAS" },
	{ 0x46, "Micro Star International" },
	{ 0xC7, "Rapport Inc." },
	{ 0xC8, "Makway International" },
	{ 0x49, "Broad Reach Engineering Co." },
	{ 0x4A, "Semiconductor Mfg Intl Corp" },
	{ 0xCB, "SiConnect" },
	{ 0x4C, "FCI USA Inc." },
	{ 0xCD, "Validity Sensors" },
	{ 0xCE, "Coney Technology Co. Ltd." },
	{ 0x4F, "Spans Logic" },
	{ 0xD0, "Neterion Inc." },
	{ 0x51, "Qimonda" },
	{ 0x52, "New Japan Radio Co. Ltd." },
	{ 0xD3, "Velogix" },
	{ 0x54, "Montalvo Systems" },
	{ 0xD5, "iVivity Inc." },
	{ 0xD6, "Walton Chaintech" },
	{ 0x57, "AENEON" },
	{ 0x58, "Lorom Industrial Co. Ltd." },
	{ 0xD9, "Radiospire Networks" },
	{ 0xDA, "Sensio Technologies, Inc." },
	{ 0x5B, "Nethra Imaging" },
	{ 0xDC, "Hexon Technology Pte Ltd" },
	{ 0x5D, "CompuStocx (CSX)" },
	{ 0x5E, "Methode Electronics, Inc." },
	{ 0xDF, "Connect One Ltd." },
	{ 0xE0, "Opulan Technologies" },
	{ 0x61, "Septentrio NV" },
	{ 0x62, "Goldenmars Technology Inc." },
	{ 0xE3, "Kreton Corporation" },
	{ 0x64, "Cochlear Ltd." },
	{ 0xE5, "Altair Semiconductor" },
	{ 0xE6, "NetEffect, Inc." },
	{ 0x67, "Spansion, Inc." },
	{ 0x68, "Taiwan Semiconductor Mfg" },
	{ 0xE9, "Emphany Systems Inc." },
	{ 0xEA, "ApaceWave Technologies" },
	{ 0x6B, "Mobilygen Corporation" },
	{ 0xEC, "Tego" },
	{ 0x6D, "Cswitch Corporation" },
	{ 0x6E, "Haier (Beijing) IC Design Co." },
	{ 0xEF, "MetaRAM" },
	{ 0x70, "Axel Electronics Co. Ltd." },
	{ 0xF1, "Tilera Corporation" },
	{ 0xF2, "Aquantia" },
	{ 0x73, "Vivace Semiconductor" },
	{ 0xF4, "Redpine Signals" },
	{ 0x75, "Octalica" },
	{ 0x76, "InterDigital Communications" },
	{ 0xF7, "Avant Technology" },
	{ 0xF8, "Asrock, Inc." },
	{ 0x79, "Availink" },
	{ 0x7A, "Quartics, Inc." },
	{ 0xFB, "Element CXI" },
	{ 0x7C, "Innovaciones Microelectronicas" },
	{ 0xFD, "VeriSilicon Microelectronics" },
	{ 0xFE, "W5 Networks" },
	{ 0x00, NULL },
};

const struct valstr jedec_id7_vals[] = {
	{ 0x01, "MOVEKING" },
	{ 0x02, "Mavrix Technology, Inc." },
	{ 0x83, "CellGuide Ltd." },
	{ 0x04, "Faraday Technology" },
	{ 0x85, "Diablo Technologies, Inc." },
	{ 0x86, "Jennic" },
	{ 0x07, "Octasic" },
	{ 0x08, "Molex Incorporated" },
	{ 0x89, "3Leaf Networks" },
	{ 0x8A, "Bright Micron Technology" },
	{ 0x0B, "Netxen" },
	{ 0x8C, "NextWave Broadband Inc." },
	{ 0x0D, "DisplayLink" },
	{ 0x0E, "ZMOS Technology" },
	{ 0x8F, "Tec-Hill" },
	{ 0x10, "Multigig, Inc." },
	{ 0x91, "Amimon" },
	{ 0x92, "Euphonic Technologies, Inc." },
	{ 0x13, "BRN Phoenix" },
	{ 0x94, "InSilica" },
	{ 0x15, "Ember Corporation" },
	{ 0x16, "Avexir Technologies Corporation" },
	{ 0x97, "Echelon Corporation" },
	{ 0x98, "Edgewater Computer Systems" },
	{ 0x19, "XMOS Semiconductor Ltd." },
	{ 0x1A, "GENUSION, Inc." },
	{ 0x9B, "Memory Corp NV" },
	{ 0x1C, "SiliconBlue Technologies" },
	{ 0x9D, "Rambus Inc." },
	{ 0x9E, "Andes Technology Corporation" },
	{ 0x1F, "Coronis Systems" },
	{ 0x20, "Achronix Semiconductor" },
	{ 0xA1, "Siano Mobile Silicon Ltd." },
	{ 0xA2, "Semtech Corporation" },
	{ 0x23, "Pixelworks Inc." },
	{ 0xA4, "Gaisler Research AB" },
	{ 0x25, "Teranetics" },
	{ 0x26, "Toppan Printing Co. Ltd." },
	{ 0xA7, "Kingxcon" },
	{ 0xA8, "Silicon Integrated Systems" },
	{ 0x29, "I-O Data Device, Inc." },
	{ 0x2A, "NDS Americas Inc." },
	{ 0xAB, "Solomon Systech Limited" },
	{ 0x2C, "On Demand Microelectronics" },
	{ 0xAD, "Amicus Wireless Inc." },
	{ 0xAE, "SMARDTV SNC" },
	{ 0x2F, "Comsys Communication Ltd." },
	{ 0xB0, "Movidia Ltd." },
	{ 0x31, "Javad GNSS, Inc." },
	{ 0x32, "Montage Technology Group" },
	{ 0xB3, "Trident Microsystems" },
	{ 0x34, "Super Talent" },
	{ 0xB5, "Optichron, Inc." },
	{ 0xB6, "Future Waves UK Ltd." },
	{ 0x37, "SiBEAM, Inc." },
	{ 0x38, "Inicore,Inc." },
	{ 0xB9, "Virident Systems" },
	{ 0xBA, "M2000, Inc." },
	{ 0x3B, "ZeroG Wireless, Inc." },
	{ 0xBC, "Gingle Technology Co. Ltd." },
	{ 0x3D, "Space Micro Inc." },
	{ 0x3E, "Wilocity" },
	{ 0xBF, "Novafora, Inc." },
	{ 0x40, "iKoa Corporation" },
	{ 0xC1, "ASint Technology" },
	{ 0xC2, "Ramtron" },
	{ 0x43, "Plato Networks Inc." },
	{ 0xC4, "IPtronics AS" },
	{ 0x45, "Infinite-Memories" },
	{ 0x46, "Parade Technologies Inc." },
	{ 0xC7, "Dune Networks" },
	{ 0xC8, "GigaDevice Semiconductor" },
	{ 0x49, "Modu Ltd." },
	{ 0x4A, "CEITEC" },
	{ 0xCB, "Northrop Grumman" },
	{ 0x4C, "XRONET Corporation" },
	{ 0xCD, "Sicon Semiconductor AB" },
	{ 0xCE, "Atla Electronics Co. Ltd." },
	{ 0x4F, "TOPRAM Technology" },
	{ 0xD0, "Silego Technology Inc." },
	{ 0x51, "Kinglife" },
	{ 0x52, "Ability Industries Ltd." },
	{ 0xD3, "Silicon Power Computer & Communications" },
	{ 0x54, "Augusta Technology, Inc." },
	{ 0xD5, "Nantronics Semiconductors" },
	{ 0xD6, "Hilscher Gesellschaft" },
	{ 0x57, "Quixant Ltd." },
	{ 0x58, "Percello Ltd." },
	{ 0xD9, "NextIO Inc." },
	{ 0xDA, "Scanimetrics Inc." },
	{ 0x5B, "FS-Semi Company Ltd." },
	{ 0xDC, "Infinera Corporation" },
	{ 0x5D, "SandForce Inc." },
	{ 0x5E, "Lexar Media" },
	{ 0xDF, "Teradyne Inc." },
	{ 0xE0, "Memory Exchange Corp." },
	{ 0x61, "Suzhou Smartek Electronics" },
	{ 0x62, "Avantium Corporation" },
	{ 0xE3, "ATP Electronics Inc." },
	{ 0x64, "Valens Semiconductor Ltd" },
	{ 0xE5, "Agate Logic, Inc." },
	{ 0xE6, "Netronome" },
	{ 0x67, "Zenverge, Inc." },
	{ 0x68, "N-trig Ltd" },
	{ 0xE9, "SanMax Technologies Inc." },
	{ 0xEA, "Contour Semiconductor Inc." },
	{ 0x6B, "TwinMOS" },
	{ 0xEC, "Silicon Systems, Inc." },
	{ 0x6D, "V-Color Technology Inc." },
	{ 0x6E, "Certicom Corporation" },
	{ 0xEF, "JSC ICC Milandr" },
	{ 0x70, "PhotoFast Global Inc." },
	{ 0xF1, "InnoDisk Corporation" },
	{ 0xF2, "Muscle Power" },
	{ 0x73, "Energy Micro" },
	{ 0xF4, "Innofidei" },
	{ 0x75, "CopperGate Communications" },
	{ 0x76, "Holtek Semiconductor Inc." },
	{ 0xF7, "Myson Century, Inc." },
	{ 0xF8, "FIDELIX" },
	{ 0x79, "Red Digital Cinema" },
	{ 0x7A, "Densbits Technology" },
	{ 0xFB, "Zempro" },
	{ 0x7C, "MoSys" },
	{ 0xFD, "Provigent" },
	{ 0xFE, "Triad Semiconductor, Inc." },
	{ 0x00, NULL },
};

const struct valstr jedec_id8_vals[] = {
	{ 0x01, "Siklu Communication Ltd." },
	{ 0x02, "A Force Manufacturing Ltd." },
	{ 0x83, "Strontium" },
	{ 0x04, "Abilis Systems" },
	{ 0x85, "Siglead, Inc." },
	{ 0x86, "Ubicom, Inc." },
	{ 0x07, "Unifosa Corporation" },
	{ 0x08, "Stretch, Inc." },
	{ 0x89, "Lantiq Deutschland GmbH" },
	{ 0x8A, "Visipro." },
	{ 0x0B, "EKMemory" },
	{ 0x8C, "Microelectronics Institute ZTE" },
	{ 0x0D, "Cognovo Ltd." },
	{ 0x0E, "Carry Technology Co. Ltd." },
	{ 0x8F, "Nokia" },
	{ 0x10, "King Tiger Technology" },
	{ 0x91, "Sierra Wireless" },
	{ 0x92, "HT Micron" },
	{ 0x13, "Albatron Technology Co. Ltd." },
	{ 0x94, "Leica Geosystems AG" },
	{ 0x15, "BroadLight" },
	{ 0x16, "AEXEA" },
	{ 0x97, "ClariPhy Communications, Inc." },
	{ 0x98, "Green Plug" },
	{ 0x19, "Design Art Networks" },
	{ 0x1A, "Mach Xtreme Technology Ltd." },
	{ 0x9B, "ATO Solutions Co. Ltd." },
	{ 0x1C, "Ramsta" },
	{ 0x9D, "Greenliant Systems, Ltd." },
	{ 0x9E, "Teikon" },
	{ 0x1F, "Antec Hadron" },
	{ 0x20, "NavCom Technology, Inc." },
	{ 0xA1, "Shanghai Fudan Microelectronics" },
	{ 0xA2, "Calxeda, Inc." },
	{ 0x23, "JSC EDC Electronics" },
	{ 0xA4, "Kandit Technology Co. Ltd." },
	{ 0x25, "Ramos Technology" },
	{ 0x26, "Goldenmars Technology" },
	{ 0xA7, "XeL Technology Inc." },
	{ 0xA8, "Newzone Corporation" },
	{ 0x29, "ShenZhen MercyPower Tech" },
	{ 0x2A, "Nanjing Yihuo Technology" },
	{ 0xAB, "Nethra Imaging Inc." },
	{ 0x2C, "SiTel Semiconductor BV" },
	{ 0xAD, "SolidGear Corporation" },
	{ 0xAE, "Topower Computer Ind Co Ltd." },
	{ 0x2F, "Wilocity" },
	{ 0xB0, "Profichip GmbH" },
	{ 0x31, "Gerad Technologies" },
	{ 0x32, "Ritek Corporation" },
	{ 0xB3, "Gomos Technology Limited" },
	{ 0x34, "Memoright Corporation" },
	{ 0xB5, "D-Broad, Inc." },
	{ 0xB6, "HiSilicon Technologies" },
	{ 0x37, "Syndiant Inc.." },
	{ 0x38, "Enverv Inc." },
	{ 0xB9, "Cognex" },
	{ 0xBA, "Xinnova Technology Inc." },
	{ 0x3B, "Ultron AG" },
	{ 0xBC, "Concord Idea Corporation" },
	{ 0x3D, "AIM Corporation" },
	{ 0x3E, "Lifetime Memory Products" },
	{ 0xBF, "Ramsway" },
	{ 0x40, "Recore Systems B.V." },
	{ 0xC1, "Haotian Jinshibo Science Tech" },
	{ 0xC2, "Being Advanced Memory" },
	{ 0x43, "Adesto Technologies" },
	{ 0xC4, "Giantec Semiconductor, Inc." },
	{ 0x45, "HMD Electronics AG" },
	{ 0x46, "Gloway International (HK)" },
	{ 0xC7, "Kingcore" },
	{ 0xC8, "Anucell Technology Holding" },
	{ 0x49, "Accord Software & Systems Pvt. Ltd." },
	{ 0x4A, "Active-Semi Inc." },
	{ 0xCB, "Denso Corporation" },
	{ 0x4C, "TLSI Inc." },
	{ 0xCD, "Qidan" },
	{ 0xCE, "Mustang" },
	{ 0x4F, "Orca Systems" },
	{ 0xD0, "Passif Semiconductor" },
	{ 0x51, "GigaDevice Semiconductor (Beijing) Inc." },
	{ 0x52, "Memphis Electronic" },
	{ 0xD3, "Beckhoff Automation GmbH" },
	{ 0x54, "Harmony Semiconductor Corp" },
	{ 0xD5, "Air Computers SRL" },
	{ 0xD6, "TMT Memory" },
	{ 0x57, "Eorex Corporation" },
	{ 0x58, "Xingtera" },
	{ 0xD9, "Netsol" },
	{ 0xDA, "Bestdon Technology Co. Ltd." },
	{ 0x5B, "Baysand Inc." },
	{ 0xDC, "Uroad Technology Co. Ltd." },
	{ 0x5D, "Wilk Elektronik S.A." },
	{ 0x5E, "AAI" },
	{ 0xDF, "Harman" },
	{ 0xE0, "Berg Microelectronics Inc." },
	{ 0x61, "ASSIA, Inc." },
	{ 0x62, "Visiontek Products LLC" },
	{ 0xE3, "OCMEMORY" },
	{ 0x64, "Welink Solution Inc." },
	{ 0xE5, "Shark Gaming" },
	{ 0xE6, "Avalanche Technology" },
	{ 0x67, "R&D Center ELVEES OJSC" },
	{ 0x68, "KingboMars Technology Co. Ltd." },
	{ 0xE9, "High Bridge Solutions Industria Eletronica" },
	{ 0xEA, "Transcend Technology Co. Ltd." },
	{ 0x6B, "Everspin Technologies" },
	{ 0xEC, "Hon-Hai Precision" },
	{ 0x6D, "Smart Storage Systems" },
	{ 0x6E, "Toumaz Group" },
	{ 0xEF, "Zentel Electronics Corporation" },
	{ 0x70, "Panram International Corporation" },
	{ 0xF1, "Silicon Space Technology" },
	{ 0xF2, "LITE-ON IT Corporation" },
	{ 0x73, "Inuitive" },
	{ 0xF4, "HMicro" },
	{ 0x75, "BittWare, Inc." },
	{ 0x76, "GLOBALFOUNDRIES" },
	{ 0xF7, "ACPI Digital Co. Ltd." },
	{ 0xF8, "Annapurna Labs" },
	{ 0x79, "AcSiP Technology Corporation" },
	{ 0x7A, "Idea! Electronic Systems" },
	{ 0xFB, "Gowe Technology Co. Ltd." },
	{ 0x7C, "Hermes Testing Solutions, Inc." },
	{ 0xFD, "Positivo BGH" },
	{ 0xFE, "Intelligence Silicon Technology" },
	{ 0x00, NULL },
};

const struct valstr jedec_id9_vals[] = {
	{ 0x01, "3D PLUS" },
	{ 0x02, "Diehl Aerospace" },
	{ 0x83, "Fairchild" },
	{ 0x04, "Mercury Systems" },
	{ 0x85, "Sonics, Inc." },
	{ 0x86, "GE Intelligent Platforms GmbH & Co." },
	{ 0x07, "Shenzhen Jinge Information Co. Ltd." },
	{ 0x08, "SCWW" },
	{ 0x89, "Silicon Motion Inc." },
	{ 0x8A, "Anurag" },
	{ 0x0B, "King Kong" },
	{ 0x8C, "FROM30 Co. Ltd." },
	{ 0x0D, "Gowin Semiconductor Corp" },
	{ 0x0E, "Fremont Micro Devices Ltd." },
	{ 0x8F, "Ericsson Modems" },
	{ 0x10, "Exelis" },
	{ 0x91, "Satixfy Ltd." },
	{ 0x92, "Galaxy Microsystems Ltd." },
	{ 0x13, "Gloway International Co. Ltd." },
	{ 0x94, "Lab" },
	{ 0x15, "Smart Energy Instruments" },
	{ 0x16, "Approved Memory Corporation" },
	{ 0x97, "Axell Corporation" },
	{ 0x98, "Essencore Limited" },
	{ 0x19, "Phytium" },
	{ 0x1A, "Xi’an SinoChip Semiconductor" },
	{ 0x9B, "Ambiq Micro" },
	{ 0x1C, "eveRAM Technology, Inc." },
	{ 0x9D, "Infomax" },
	{ 0x9E, "Butterfly Network, Inc." },
	{ 0x1F, "Shenzhen City Gcai Electronics" },
	{ 0x20, "Stack Devices Corporation" },
	{ 0xA1, "ADK Media Group" },
	{ 0xA2, "TSP Global Co., Ltd." },
	{ 0x23, "HighX" },
	{ 0xA4, "Shenzhen Elicks Technology" },
	{ 0x25, "ISSI/Chingis" },
	{ 0x26, "Google, Inc." },
	{ 0xA7, "Dasima International Development" },
	{ 0xA8, "Leahkinn Technology Limited" },
	{ 0x29, "HIMA Paul Hildebrandt GmbH Co KG" },
	{ 0x2A, "Keysight Technologies" },
	{ 0xAB, "Techcomp International (Fastable)" },
	{ 0x2C, "Ancore Technology Corporation" },
	{ 0xAD, "Nuvoton" },
	{ 0xAE, "Korea Uhbele International Group Ltd." },
	{ 0x2F, "Ikegami Tsushinki Co Ltd." },
	{ 0xB0, "RelChip, Inc." },
	{ 0x31, "Baikal Electronics" },
	{ 0x32, "Nemostech Inc." },
	{ 0xB3, "Memorysolution GmbH" },
	{ 0x34, "Silicon Integrated Systems Corporation" },
	{ 0xB5, "Xiede" },
	{ 0xB6, "Multilaser Components" },
	{ 0x37, "Flash Chi" },
	{ 0x38, "Jone" },
	{ 0xB9, "GCT Semiconductor Inc." },
	{ 0xBA, "Hong Kong Zetta Device Technology" },
	{ 0x3B, "Unimemory Technology(s) Pte Ltd." },
	{ 0xBC, "Cuso" },
	{ 0x3D, "Kuso" },
	{ 0x3E, "Uniquify Inc." },
	{ 0xBF, "Skymedi Corporation" },
	{ 0x40, "Core Chance Co. Ltd." },
	{ 0xC1, "Tekism Co. Ltd." },
	{ 0xC2, "Seagate Technology PLC" },
	{ 0x43, "Hong Kong Gaia Group Co. Limited" },
	{ 0xC4, "Gigacom Semiconductor LLC" },
	{ 0x45, "V2 Technologies" },
	{ 0x46, "TLi" },
	{ 0xC7, "Neotion" },
	{ 0xC8, "Lenovo" },
	{ 0x49, "Shenzhen Zhongteng Electronic Corp. Ltd." },
	{ 0x4A, "Compound Photonics" },
	{ 0xCB, "in2H2 inc" },
	{ 0x4C, "Shenzhen Pango Microsystems Co. Ltd" },
	{ 0xCD, "Vasekey" },
	{ 0xCE, "Cal-Comp Industria de Semicondutores" },
	{ 0x4F, "Eyenix Co., Ltd." },
	{ 0xD0, "Heoriady" },
	{ 0x51, "Accelerated Memory Production Inc." },
	{ 0x52, "INVECAS, Inc." },
	{ 0xD3, "AP Memory" },
	{ 0x54, "Douqi Technology" },
	{ 0xD5, "Etron Technology, Inc." },
	{ 0xD6, "Indie Semiconductor" },
	{ 0x57, "Socionext Inc." },
	{ 0x58, "HGST" },
	{ 0xD9, "EVGA" },
	{ 0xDA, "Audience Inc." },
	{ 0x5B, "EpicGear" },
	{ 0xDC, "Vitesse Enterprise Co." },
	{ 0x5D, "Foxtronn International Corporation" },
	{ 0x5E, "Bretelon Inc." },
	{ 0xDF, "Zbit Semiconductor, Inc." },
	{ 0xE0, "Eoplex Inc" },
	{ 0x61, "MaxLinear, Inc." },
	{ 0x62, "ETA Devices" },
	{ 0xE3, "LOKI" },
	{ 0x64, "IMS Semiconductor Co., Ltd" },
	{ 0xE5, "Dosilicon Co., Ltd." },
	{ 0xE6, "Dolphin Integration" },
	{ 0x67, "Shenzhen Mic Electronics Technology" },
	{ 0x68, "Boya Microelectronics Inc." },
	{ 0xE9, "Geniachip (Roche)" },
	{ 0xEA, "Axign" },
	{ 0x6B, "Kingred Electronic Technology Ltd." },
	{ 0xEC, "Chao Yue Zhuo Computer Business Dept." },
	{ 0x6D, "Guangzhou Si Nuo Electronic Technology." },
	{ 0x00, NULL },
};

int
ipmi_spd_print(uint8_t *spd_data, int len)
{
	int k = 0;
	int ii = 0;

	if (len < 92)
		return -1; /* we need first 91 bytes to do our thing */

	printf(" Memory Type           : %s\n",
	       val2str(spd_data[2], spd_memtype_vals));

	if (spd_data[2] == 0x0B)	/* DDR3 SDRAM */
	{
		int iPN;
		int sdram_cap = 0;
		int pri_bus_width = 0;
		int sdram_width = 0;
		int ranks = 0;
		int mem_size = 0;

		if (len < 148)
			return -1; /* we need first 91 bytes to do our thing */


		sdram_cap = ldexp(256,(spd_data[4]&15));
		pri_bus_width = ldexp(8,(spd_data[8]&7));
		sdram_width = ldexp(4,(spd_data[7]&7));
		ranks = ldexp(1,((spd_data[7]&0x3F)>>3));
		mem_size = (sdram_cap/8) * (pri_bus_width/sdram_width) * ranks;
		printf(" SDRAM Capacity        : %d MB\n", sdram_cap );
		printf(" Memory Banks          : %s\n", val2str(spd_data[4]>>4, ddr3_banks_vals));
		printf(" Primary Bus Width     : %d bits\n", pri_bus_width );
		printf(" SDRAM Device Width    : %d bits\n", sdram_width );
		printf(" Number of Ranks       : %d\n", ranks );
		printf(" Memory size           : %d MB\n", mem_size );

		/* printf(" Memory Density        : %s\n", val2str(spd_data[4]&15, ddr3_density_vals)); */
		printf(" 1.5 V Nominal Op      : %s\n", (((spd_data[6]&1) != 0) ? "No":"Yes" ) );
		printf(" 1.35 V Nominal Op     : %s\n", (((spd_data[6]&2) != 0) ? "No":"Yes" ) );
		printf(" 1.2X V Nominal Op     : %s\n", (((spd_data[6]&4) != 0) ? "No":"Yes" ) );
		printf(" Error Detect/Cor      : %s\n", val2str(spd_data[8]>>3, ddr3_ecc_vals));

		printf(" Manufacturer          : ");
		switch (spd_data[117]&127)
		{
		case	0:
			printf("%s\n", val2str(spd_data[118], jedec_id1_vals));
			break;

		case	1:
			printf("%s\n", val2str(spd_data[118], jedec_id2_vals));
			break;

		case	2:
			printf("%s\n", val2str(spd_data[118], jedec_id3_vals));
			break;

		case	3:
			printf("%s\n", val2str(spd_data[118], jedec_id4_vals));
			break;

		case	4:
			printf("%s\n", val2str(spd_data[118], jedec_id5_vals));
			break;

		case	5:
			printf("%s\n", val2str(spd_data[118], jedec_id6_vals));
			break;

		case	6:
			printf("%s\n", val2str(spd_data[118], jedec_id7_vals));
			break;

		case	7:
			printf("%s\n", val2str(spd_data[118], jedec_id8_vals));
			break;

		case	8:
			printf("%s\n", val2str(spd_data[118], jedec_id9_vals));
			break;

		default:
			printf("%s\n", "JEDEC JEP106 update required" );

		}

		printf(" Manufacture Date      : year %c%c week %c%c\n",
		'0'+(spd_data[120]>>4), '0'+(spd_data[120]&15), '0'+(spd_data[121]>>4), '0'+(spd_data[121]&15) );

		printf(" Serial Number         : %02x%02x%02x%02x\n",
		spd_data[122], spd_data[123], spd_data[124], spd_data[125]);

		printf(" Part Number           : ");
		for (iPN = 128; iPN < 146; iPN++) {
			printf("%c", spd_data[iPN]);
		}
		printf("\n");
	} else if (spd_data[2] == 0x0C)	/* DDR4 SDRAM */
	{
		int i;
		int sdram_cap = 0;
		int pri_bus_width = 0;
		int sdram_width = 0;
		int mem_size = 0;
		int lrank_dimm;
		uint32_t year;
		uint32_t week;

		if (len < 348)
			return -1;

		/* "Logical rank" referes to the individually addressable die
		 * in a 3DS stack and has no meaning for monolithic or
		 * multi-load stacked SDRAMs; however, for the purposes of
		 * calculating the capacity of the module, one should treat
		 * monolithic and multi-load stack SDRAMs as having one logical
		 * rank per package rank.
		 */
		lrank_dimm = (spd_data[12]>>3&0x3) + 1; /* Number of Package Ranks per DIMM */
		if ((spd_data[6] & 0x3) == 0x2) { /* 3DS package Type */
			lrank_dimm *= ((spd_data[6]>>4)&0x3) + 1; /* Die Count */
		}
		sdram_cap = ldexp(256,(spd_data[4]&15));
		pri_bus_width = ldexp(8,(spd_data[13]&7));
		sdram_width = ldexp(4,(spd_data[12]&7));
		mem_size = (sdram_cap/8) * (pri_bus_width/sdram_width) * lrank_dimm;
		printf(" SDRAM Package Type    : %s\n", val2str((spd_data[6]>>7), ddr4_package_type));
		printf(" Technology            : %s\n", val2str((spd_data[3]&15), ddr4_technology_type));
		printf(" SDRAM Die Count       : %d\n", ((spd_data[6]>>4) & 3)+1);
		printf(" SDRAM Capacity        : %d Mb\n", sdram_cap );
		printf(" Memory Bank Group     : %s\n", val2str((spd_data[4]>>6 & 0x3), ddr4_bank_groups));
		printf(" Memory Banks          : %s\n", val2str((spd_data[4]>>4 & 0x3), ddr4_banks_vals));
		printf(" Primary Bus Width     : %d bits\n", pri_bus_width );
		printf(" SDRAM Device Width    : %d bits\n", sdram_width );
		printf(" Logical Rank per DIMM : %d\n", lrank_dimm );
		printf(" Memory size           : %d MB\n", mem_size );

		printf(" Memory Density        : %s\n", val2str(spd_data[4]&15, ddr4_density_vals));
		printf(" 1.2 V Nominal Op      : %s\n", (((spd_data[11]&3) != 3) ? "No":"Yes" ) );
		printf(" TBD1 V Nominal Op     : %s\n", (((spd_data[11]>>2&3) != 3) ? "No":"Yes" ) );
		printf(" TBD2 V Nominal Op     : %s\n", (((spd_data[11]>>4&3) != 3) ? "No":"Yes" ) );
		printf(" Error Detect/Cor      : %s\n", val2str(spd_data[13]>>3, ddr4_ecc_vals));

		printf(" Manufacturer          : ");
		switch (spd_data[320]&127)
		{
		case	0:
			printf("%s\n", val2str(spd_data[321], jedec_id1_vals));
			break;

		case	1:
			printf("%s\n", val2str(spd_data[321], jedec_id2_vals));
			break;

		case	2:
			printf("%s\n", val2str(spd_data[321], jedec_id3_vals));
			break;

		case	3:
			printf("%s\n", val2str(spd_data[321], jedec_id4_vals));
			break;

		case	4:
			printf("%s\n", val2str(spd_data[321], jedec_id5_vals));
			break;

		case	5:
			printf("%s\n", val2str(spd_data[321], jedec_id6_vals));
			break;

		case	6:
			printf("%s\n", val2str(spd_data[321], jedec_id7_vals));
			break;

		case	7:
			printf("%s\n", val2str(spd_data[321], jedec_id8_vals));
			break;

		case	8:
			printf("%s\n", val2str(spd_data[321], jedec_id9_vals));
			break;

		default:
			printf("%s\n", "JEDEC JEP106 update required");

		}

		year = ((spd_data[323] >> 4) * 10) + (spd_data[323] & 15);
		week = ((spd_data[324]>>4) * 10) + (spd_data[324] & 15);
		printf(" Manufacture Date      : year %4d week %2d\n",
		       2000 + year, week);

		printf(" Serial Number         : %02x%02x%02x%02x\n",
		spd_data[325], spd_data[326], spd_data[327], spd_data[328]);

		printf(" Part Number           : ");
		for (i=329; i <= 348; i++)
		{
			printf( "%c", spd_data[i]);
		}
		printf("\n");
	}
	else
	{
		if (len < 100) {
			return (-1);
		}
		ii = (spd_data[3] & 0x0f) + (spd_data[4] & 0x0f) - 17;
		k = ((spd_data[5] & 0x7) + 1) * spd_data[17];

		if(ii > 0 && ii <= 12 && k > 0) {
			printf(" Memory Size           : %d MB\n", ((1 << ii) * k));
		} else {
			printf(" Memory Size    INVALID: %d, %d, %d, %d\n", spd_data[3],
					spd_data[4], spd_data[5], spd_data[17]);
		}
		printf(" Voltage Intf          : %s\n",
		val2str(spd_data[8], spd_voltage_vals));
		printf(" Error Detect/Cor      : %s\n",
		val2str(spd_data[11], spd_config_vals));

		/* handle jedec table bank continuation values */
		printf(" Manufacturer          : ");
		if (spd_data[64] != 0x7f)
			printf("%s\n",
			val2str(spd_data[64], jedec_id1_vals));
		else {
			if (spd_data[65] != 0x7f)
				printf("%s\n",
				val2str(spd_data[65], jedec_id2_vals));
			else {
				if (spd_data[66] != 0x7f)
					printf("%s\n",
					val2str(spd_data[66], jedec_id3_vals));
				else {
					if (spd_data[67] != 0x7f)
						printf("%s\n",
						val2str(spd_data[67], jedec_id4_vals));
					else {
						if (spd_data[68] != 0x7f)
							printf("%s\n",
							val2str(spd_data[68], jedec_id5_vals));
						else {
							if (spd_data[69] != 0x7f)
								printf("%s\n",
								val2str(spd_data[69], jedec_id6_vals));
							else {
								if (spd_data[70] != 0x7f)
									printf("%s\n",
									val2str(spd_data[70], jedec_id7_vals));
								else {
									if (spd_data[71] != 0x7f)
										printf("%s\n",
										val2str(spd_data[71], jedec_id8_vals));
									else
										printf("%s\n",
										val2str(spd_data[72], jedec_id9_vals));
								}
							}
						}
					}
				}
			}
		}

		if (spd_data[73]) {
			char part[19];
			memcpy(part, spd_data+73, 18);
			part[18] = 0;
			printf(" Part Number           : %s\n", part);
		}

		printf(" Serial Number         : %02x%02x%02x%02x\n",
		spd_data[95], spd_data[96], spd_data[97], spd_data[98]);
	}

	if (verbose) {
		printf("\n");
		printbuf(spd_data, len, "SPD DATA");
	}

	return 0;
}

int
ipmi_spd_print_fru(struct ipmi_intf * intf, uint8_t id)
{
	struct ipmi_rs * rsp;
	struct ipmi_rq req;
	struct fru_info fru;
	uint8_t *spd_data = NULL;
	uint8_t msg_data[4];
	uint32_t len, offset;
	int rc = -1;

	msg_data[0] = id;

	memset(&req, 0, sizeof(req));
	req.msg.netfn = IPMI_NETFN_STORAGE;
	req.msg.cmd = GET_FRU_INFO;
	req.msg.data = msg_data;
	req.msg.data_len = 1;

	rsp = intf->sendrecv(intf, &req);
	if (!rsp) {
		printf(" Device not present (No Response)\n");
		goto end;
	}
	if (rsp->ccode) {
		printf(" Device not present (%s)\n",
		       val2str(rsp->ccode, completion_code_vals));
		goto end;
	}

	fru.size = (rsp->data[1] << 8) | rsp->data[0];
	fru.access = rsp->data[2] & 0x1;

	lprintf(LOG_DEBUG, "fru.size = %d bytes (accessed by %s)",
		fru.size, fru.access ? "words" : "bytes");


	if (fru.size < 1) {
		lprintf(LOG_ERR, " Invalid FRU size %d", fru.size);
		goto end;
	}

        spd_data = malloc(fru.size);

        if (!spd_data) {
		printf(" Unable to malloc memory for spd array of size=%d\n",
		       fru.size);
		goto end;
        }

	memset(&req, 0, sizeof(req));
	req.msg.netfn = IPMI_NETFN_STORAGE;
	req.msg.cmd = GET_FRU_DATA;
	req.msg.data = msg_data;
	req.msg.data_len = 4;

	offset = 0;
	memset(spd_data, 0, fru.size);
	do {
		msg_data[0] = id;
		msg_data[1] = offset & 0xFF;
		msg_data[2] = offset >> 8;
		msg_data[3] = FRU_DATA_RQST_SIZE;

		rsp = intf->sendrecv(intf, &req);
		if (!rsp) {
			printf(" Device not present (No Response)\n");
			goto end;
		}
		if (rsp->ccode) {
			printf(" Device not present (%s)\n",
			       val2str(rsp->ccode, completion_code_vals));

			/* Timeouts are acceptable. No DIMM in the socket */
			if (rsp->ccode == 0xc3)
				rc = 1;

			goto end;
		}

		len = rsp->data[0];
		if(rsp->data_len < 1
		   || len > rsp->data_len - 1
		   || len > fru.size - offset)
		{
			printf(" Not enough buffer size");
			goto end;
		}
		memcpy(&spd_data[offset], rsp->data + 1, len);
		offset += len;
	} while (offset < fru.size);

	/* now print spd info */
	ipmi_spd_print(spd_data, offset);
	rc = 0;

end:
	free_n(&spd_data);

	return rc;
}
