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

#include <ipmitool/ipmi.h>

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
	unsigned char area_len;
	unsigned char type;
	char * part;
	char * serial;
};

struct fru_area_board {
	unsigned char	area_ver;
	unsigned char area_len;
	unsigned char lang;
	unsigned long mfg_date_time;
	char * mfg;
	char * prod;
	char * serial;
	char * part;
};

struct fru_area_product {
	unsigned char area_ver;
	unsigned char area_len;
	unsigned char lang;
	char * mfg;
	char * name;
	char * part;
	char * version;
	char * serial;
	char * asset;
};

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

void ipmi_print_fru(struct ipmi_intf *, unsigned char);
int ipmi_fru_main(struct ipmi_intf *, int, char **);

#endif /* IPMI_FRU_H */
