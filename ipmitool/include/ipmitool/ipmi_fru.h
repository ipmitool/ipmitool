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

#ifndef IPMI_FRU_H
#define IPMI_FRU_H

#include <inttypes.h>
#include <ipmitool/ipmi.h>
#include <ipmitool/ipmi_sdr.h>

#if HAVE_CONFIG_H
# include <config.h>
#endif

#define GET_FRU_INFO		0x10
#define GET_FRU_DATA		0x11
#define SET_FRU_DATA		0x12

enum {
	FRU_CHASSIS_PARTNO,
	FRU_CHASSIS_SERIAL,
	FRU_BOARD_MANUF,
	FRU_BOARD_PRODUCT,
	FRU_BOARD_SERIAL,
	FRU_BOARD_PARTNO,
	FRU_PRODUCT_MANUF,
	FRU_PRODUCT_NAME,
	FRU_PRODUCT_PARTNO,
	FRU_PRODUCT_VERSION,
	FRU_PRODUCT_SERIAL,
	FRU_PRODUCT_ASSET,
};

struct fru_info {
	unsigned short size;
	unsigned char access : 1;
} __attribute__ ((packed));

struct fru_header {
	unsigned char version;
	struct {
		unsigned char internal;
		unsigned char chassis;
		unsigned char board;
		unsigned char product;
		unsigned char multi;
	} offset;
	unsigned char pad;
	unsigned char checksum;
} __attribute__ ((packed));

struct fru_area_chassis {
	unsigned char area_ver;
	unsigned char type;
	unsigned short area_len;
	char * part;
	char * serial;
};

struct fru_area_board {
	unsigned char area_ver;
	unsigned char lang;
	unsigned short area_len;
	uint32_t mfg_date_time;
	char * mfg;
	char * prod;
	char * serial;
	char * part;
	char * fru;
};

struct fru_area_product {
	unsigned char area_ver;
	unsigned char lang;
	unsigned short area_len;
	char * mfg;
	char * name;
	char * part;
	char * version;
	char * serial;
	char * asset;
	char * fru;
};

struct fru_multirec_header {
#define FRU_RECORD_TYPE_POWER_SUPPLY_INFORMATION 0x00
#define FRU_RECORD_TYPE_DC_OUTPUT 0x01
#define FRU_RECORD_TYPE_DC_LOAD 0x02
#define FRU_RECORD_TYPE_MANAGEMENT_ACCESS 0x03
#define FRU_RECORD_TYPE_BASE_COMPATIBILITY 0x04
#define FRU_RECORD_TYPE_EXTENDED_COMPATIBILITY 0x05
	unsigned char type;
	unsigned char format;
	unsigned char len;
	unsigned char record_checksum;
	unsigned char header_checksum;
} __attribute__ ((packed));

struct fru_multirec_powersupply {
#if WORDS_BIGENDIAN
	unsigned short capacity;
#else
	unsigned short capacity		: 12;
	unsigned short __reserved1	: 4;
#endif
	unsigned short peak_va;
	unsigned char  inrush_current;
	unsigned char  inrush_interval;
	unsigned short lowend_input1;
	unsigned short highend_input1;
	unsigned short lowend_input2;
	unsigned short highend_input2;
	unsigned char  lowend_freq;
	unsigned char  highend_freq;
	unsigned char  dropout_tolerance;
#if WORDS_BIGENDIAN
	unsigned char  __reserved2	: 3;
	unsigned char  tach		: 1;
	unsigned char  hotswap		: 1;
	unsigned char  autoswitch	: 1;
	unsigned char  pfc		: 1;
	unsigned char  predictive_fail	: 1;
#else
	unsigned char  predictive_fail	: 1;
	unsigned char  pfc		: 1;
	unsigned char  autoswitch	: 1;
	unsigned char  hotswap		: 1;
	unsigned char  tach		: 1;
	unsigned char  __reserved2	: 3;
#endif
	unsigned short peak_cap_ht;
#if WORDS_BIGENDIAN
	unsigned char  combined_voltage1 : 4;
	unsigned char  combined_voltage2 : 4;
#else
	unsigned char  combined_voltage2 : 4;
	unsigned char  combined_voltage1 : 4;
#endif
	unsigned short combined_capacity;
	unsigned char  rps_threshold;
} __attribute__ ((packed));

static const char * combined_voltage_desc[] __attribute__((unused)) = {
	"12 V", "-12 V", "5 V", "3.3 V"
};

struct fru_multirec_dcoutput {
#if WORDS_BIGENDIAN
	unsigned char  standby		: 1;
	unsigned char  __reserved	: 3;
	unsigned char  output_number	: 4;
#else
	unsigned char  output_number	: 4;
	unsigned char  __reserved	: 3;
	unsigned char  standby		: 1;
#endif
	short nominal_voltage;
	short max_neg_dev;
	short max_pos_dev;
	unsigned short ripple_and_noise;
	unsigned short min_current;
	unsigned short max_current;
} __attribute__ ((packed));

struct fru_multirec_dcload {
#if WORDS_BIGENDIAN
	unsigned char  __reserved	: 4;
	unsigned char  output_number	: 4;
#else
	unsigned char  output_number	: 4;
	unsigned char  __reserved	: 4;
#endif
	short nominal_voltage;
	short min_voltage;
	short max_voltage;
	unsigned short ripple_and_noise;
	unsigned short min_current;
	unsigned short max_current;
} __attribute__ ((packed));

static const char * chassis_type_desc[] __attribute__((unused)) = {
	"Unspecified", "Other", "Unknown",
	"Desktop", "Low Profile Desktop", "Pizza Box",
	"Mini Tower", "Tower",
	"Portable", "LapTop", "Notebook", "Hand Held", "Docking Station",
	"All in One", "Sub Notebook", "Space-saving", "Lunch Box",
	"Main Server Chassis", "Expansion Chassis", "SubChassis",
	"Bus Expansion Chassis", "Peripheral Chassis", "RAID Chassis",
	"Rack Mount Chassis"
};

int ipmi_fru_main(struct ipmi_intf * intf, int argc, char ** argv);
int ipmi_fru_print(struct ipmi_intf * intf, struct sdr_record_fru_locator * fru);

#endif /* IPMI_FRU_H */
