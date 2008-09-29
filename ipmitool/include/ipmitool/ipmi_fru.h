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
	uint16_t size;
	uint8_t access:1;
} __attribute__ ((packed));

struct fru_header {
	uint8_t version;
	struct {
		uint8_t internal;
		uint8_t chassis;
		uint8_t board;
		uint8_t product;
		uint8_t multi;
	} offset;
	uint8_t pad;
	uint8_t checksum;
} __attribute__ ((packed));

struct fru_area_chassis {
	uint8_t area_ver;
	uint8_t type;
	uint16_t area_len;
	char * part;
	char * serial;
};

struct fru_area_board {
	uint8_t area_ver;
	uint8_t lang;
	uint16_t area_len;
	uint32_t mfg_date_time;
	char * mfg;
	char * prod;
	char * serial;
	char * part;
	char * fru;
};

struct fru_area_product {
	uint8_t area_ver;
	uint8_t lang;
	uint16_t area_len;
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
#define FRU_RECORD_TYPE_OEM_EXTENSION	0xc0
	uint8_t type;
	uint8_t format;
	uint8_t len;
	uint8_t record_checksum;
	uint8_t header_checksum;
} __attribute__ ((packed));

struct fru_multirec_powersupply {
#if WORDS_BIGENDIAN
	uint16_t capacity;
#else
	uint16_t capacity:12;
	uint16_t __reserved1:4;
#endif
	uint16_t peak_va;
	uint8_t inrush_current;
	uint8_t inrush_interval;
	uint16_t lowend_input1;
	uint16_t highend_input1;
	uint16_t lowend_input2;
	uint16_t highend_input2;
	uint8_t lowend_freq;
	uint8_t highend_freq;
	uint8_t dropout_tolerance;
#if WORDS_BIGENDIAN
	uint8_t __reserved2:3;
	uint8_t tach:1;
	uint8_t hotswap:1;
	uint8_t autoswitch:1;
	uint8_t pfc:1;
	uint8_t predictive_fail:1;
#else
	uint8_t predictive_fail:1;
	uint8_t pfc:1;
	uint8_t autoswitch:1;
	uint8_t hotswap:1;
	uint8_t tach:1;
	uint8_t __reserved2:3;
#endif
	uint16_t peak_cap_ht;
#if WORDS_BIGENDIAN
	uint8_t combined_voltage1:4;
	uint8_t combined_voltage2:4;
#else
	uint8_t combined_voltage2:4;
	uint8_t combined_voltage1:4;
#endif
	uint16_t combined_capacity;
	uint8_t rps_threshold;
} __attribute__ ((packed));

static const char * combined_voltage_desc[] __attribute__((unused)) = {
"12 V", "-12 V", "5 V", "3.3 V"};

struct fru_multirec_dcoutput {
#if WORDS_BIGENDIAN
	uint8_t standby:1;
	uint8_t __reserved:3;
	uint8_t output_number:4;
#else
	uint8_t output_number:4;
	uint8_t __reserved:3;
	uint8_t standby:1;
#endif
	short nominal_voltage;
	short max_neg_dev;
	short max_pos_dev;
	uint16_t ripple_and_noise;
	uint16_t min_current;
	uint16_t max_current;
} __attribute__ ((packed));

struct fru_multirec_dcload {
#if WORDS_BIGENDIAN
	uint8_t __reserved:4;
	uint8_t output_number:4;
#else
	uint8_t output_number:4;
	uint8_t __reserved:4;
#endif
	short nominal_voltage;
	short min_voltage;
	short max_voltage;
	uint16_t ripple_and_noise;
	uint16_t min_current;
	uint16_t max_current;
} __attribute__ ((packed));

struct fru_multirec_oem_header {
	unsigned char mfg_id[3];
#define FRU_PICMG_BACKPLANE_P2P			0x04
#define FRU_PICMG_ADDRESS_TABLE			0x10
#define FRU_PICMG_SHELF_POWER_DIST		0x11
#define FRU_PICMG_SHELF_ACTIVATION		0x12
#define FRU_PICMG_SHMC_IP_CONN			0x13
#define FRU_PICMG_BOARD_P2P				0x14
#define FRU_AMC_CURRENT					0x16
#define FRU_AMC_ACTIVATION				0x17
#define FRU_AMC_CARRIER_P2P				0x18
#define FRU_AMC_P2P						0x19
#define FRU_AMC_CARRIER_INFO			0x1a
#define FRU_UTCA_FRU_INFO_TABLE			0x20
#define FRU_UTCA_CARRIER_MNG_IP			0x21
#define FRU_UTCA_CARRIER_INFO			0x22
#define FRU_UTCA_CARRIER_LOCATION		0x23
#define FRU_UTCA_SHMC_IP_LINK			0x24
#define FRU_UTCA_POWER_POLICY			0x25
#define FRU_UTCA_ACTIVATION				0x26
#define FRU_UTCA_PM_CAPABILTY			0x27
#define FRU_UTCA_FAN_GEOGRAPHY			0x28
#define FRU_UTCA_CLOCK_MAPPING			0x29
#define FRU_UTCA_MSG_BRIDGE_POLICY		0x2A
#define FRU_UTCA_OEM_MODULE_DESC		0x2B
#define FRU_PICMG_CLK_CARRIER_P2P		0x2C
#define FRU_PICMG_CLK_CONFIG			0x2D
	unsigned char record_id;
	unsigned char record_version;
} __attribute__ ((packed));

struct fru_picmgext_guid {
	unsigned char guid[16];
} __attribute__ ((packed));

struct fru_picmgext_link_desc {
#ifndef WORDS_BIGENDIAN
	unsigned int desig_channel:6;
	unsigned int desig_if:2;
	unsigned int desig_port:4;
#define FRU_PICMGEXT_LINK_TYPE_BASE					0x01
#define FRU_PICMGEXT_LINK_TYPE_FABRIC_ETHERNET		0x02
#define FRU_PICMGEXT_LINK_TYPE_FABRIC_INFINIBAND	0x03
#define FRU_PICMGEXT_LINK_TYPE_FABRIC_STAR			0x04
#define FRU_PICMGEXT_LINK_TYPE_PCIE					0x05
	unsigned int type:8;
	unsigned int ext:4;
	unsigned int grouping:8;
#else
	unsigned int grouping:8;
	unsigned int ext:4;
#define FRU_PICMGEXT_LINK_TYPE_BASE					0x01
#define FRU_PICMGEXT_LINK_TYPE_FABRIC_ETHERNET		0x02
#define FRU_PICMGEXT_LINK_TYPE_FABRIC_INFINIBAND	0x03
#define FRU_PICMGEXT_LINK_TYPE_FABRIC_STAR			0x04
#define FRU_PICMGEXT_LINK_TYPE_PCIE					0x05
	unsigned int type:8;
	unsigned int desig_port:4;
	unsigned int desig_if:2;
	unsigned int desig_channel:6;
#endif
} __attribute__ ((packed));


#define FRU_PICMGEXT_AMC_LINK_TYPE_RESERVED			 	  0x00
#define FRU_PICMGEXT_AMC_LINK_TYPE_RESERVED1            0x01
#define FRU_PICMGEXT_AMC_LINK_TYPE_PCI_EXPRESS          0x02
#define FRU_PICMGEXT_AMC_LINK_TYPE_ADVANCED_SWITCHING1  0x03
#define FRU_PICMGEXT_AMC_LINK_TYPE_ADVANCED_SWITCHING2  0x04
#define FRU_PICMGEXT_AMC_LINK_TYPE_ETHERNET             0x05
#define FRU_PICMGEXT_AMC_LINK_TYPE_RAPIDIO              0x06
#define FRU_PICMGEXT_AMC_LINK_TYPE_STORAGE              0x07

/* This is used in command, not in FRU */
struct fru_picmgext_amc_link_info {
   unsigned char linkInfo[3];
} __attribute__ ((packed));

struct fru_picmgext_amc_link_desc_core {
#ifndef WORDS_BIGENDIAN
	unsigned int designator:12;
	unsigned int type:8;
	unsigned int ext:4;
	unsigned int grouping:8;
#else
	unsigned int grouping:8;
	unsigned int ext:4;
	unsigned int type:8;
	unsigned int designator:12;
#endif
} __attribute__ ((packed));

struct fru_picmgext_amc_link_desc_extra {
#ifndef WORDS_BIGENDIAN
	unsigned char asymetricMatch:2;
	unsigned char reserved:6;
#else
	unsigned char reserved:6;
	unsigned char asymetricMatch:2;
#endif
} __attribute__ ((packed));


struct fru_picmgext_amc_link_desc {
#ifndef WORDS_BIGENDIAN
   struct fru_picmgext_amc_link_desc_core  core;/* lsb */
   struct fru_picmgext_amc_link_desc_extra extra;
#else
   struct fru_picmgext_amc_link_desc_extra extra;
   struct fru_picmgext_amc_link_desc_core  core;/* lsb */
#endif
} __attribute__ ((packed));


#define FRU_PICMGEXT_OEM_SWFW 0x03
#define OEM_SWFW_NBLOCK_OFFSET 0x05
#define OEM_SWFW_FIELD_START_OFFSET 0x06


struct fru_picmgext_chn_desc {
#ifndef WORDS_BIGENDIAN
	unsigned char remote_slot:8;
	unsigned char remote_chn:5;
	unsigned char local_chn:5;
	unsigned char:6;
#else
	unsigned char:6;
	unsigned char local_chn:5;
	unsigned char remote_chn:5;
	unsigned char remote_slot:8;
#endif
} __attribute__ ((packed));

struct fru_picmgext_slot_desc {
	unsigned char chan_type;
	unsigned char slot_addr;
	unsigned char chn_count;
} __attribute__ ((packed));

#define FRU_PICMGEXT_DESIGN_IF_BASE				0x00
#define FRU_PICMGEXT_DESIGN_IF_FABRIC			0x01
#define FRU_PICMGEXT_DESIGN_IF_UPDATE_CHANNEL	0x02
#define FRU_PICMGEXT_DESIGN_IF_RESERVED			0x03

struct fru_picmgext_carrier_activation_record {
	unsigned short max_internal_curr;
	unsigned char  allowance_for_readiness;
   unsigned char  module_activation_record_count;
} __attribute__ ((packed));

struct fru_picmgext_activation_record {
	unsigned char ibmb_addr;
	unsigned char max_module_curr;
	unsigned char reserved;
} __attribute__ ((packed));

struct fru_picmgext_carrier_p2p_record {
	unsigned char resource_id;
	unsigned char p2p_count;
} __attribute__ ((packed));

struct fru_picmgext_carrier_p2p_descriptor {
#ifndef WORDS_BIGENDIAN
	unsigned char  remote_resource_id;
	unsigned short remote_port:5;
	unsigned short local_port:5;
	unsigned short reserved:6;
#else
	unsigned short reserved:6;
	unsigned short local_port:5;
	unsigned short remote_port:5;
	unsigned char  remote_resource_id;
#endif
} __attribute__ ((packed));

struct fru_picmgext_amc_p2p_record {
#ifndef WORDS_BIGENDIAN
	unsigned char resource_id         :4;
	unsigned char /* reserved */      :3;
	unsigned char record_type         :1;
#else	
	unsigned char record_type         :1;
	unsigned char /* reserved */      :3;
	unsigned char resource_id         :4;
#endif 
} __attribute__ ((packed));

struct fru_picmgext_amc_channel_desc_record {
#ifndef WORDS_BIGENDIAN
	unsigned char lane0port           :5;
	unsigned char lane1port           :5;
	unsigned char lane2port           :5;
	unsigned char lane3port           :5;
	unsigned char  /*reserved */     :4;
#else	
	unsigned char /*reserved  */     :4;
	unsigned char lane3port           :5;
	unsigned char lane2port           :5;
	unsigned char lane1port           :5;
	unsigned char lane0port           :5;
#endif 
} __attribute__ ((packed));

struct fru_picmgext_amc_link_desc_record {
	#define FRU_PICMGEXT_AMC_LINK_TYPE_PCIE		0x02
	#define FRU_PICMGEXT_AMC_LINK_TYPE_PCIE_AS1	0x03
	#define FRU_PICMGEXT_AMC_LINK_TYPE_PCIE_AS2	0x04
	#define FRU_PICMGEXT_AMC_LINK_TYPE_ETHERNET	0x05		
	#define FRU_PICMGEXT_AMC_LINK_TYPE_RAPIDIO	0x06
	#define FRU_PICMGEXT_AMC_LINK_TYPE_STORAGE	0x07
	
	#define AMC_LINK_TYPE_EXT_PCIE_G1_NSSC	0x00
	#define AMC_LINK_TYPE_EXT_PCIE_G1_SSC	0x01
	#define AMC_LINK_TYPE_EXT_PCIE_G2_NSSC	0x02
	#define AMC_LINK_TYPE_EXT_PCIE_G2_SSC	0x03

	#define AMC_LINK_TYPE_EXT_ETH_1000_BX	0x00
	#define AMC_LINK_TYPE_EXT_ETH_10G_XAUI	0x01
	
	#define AMC_LINK_TYPE_EXT_STORAGE_FC	0x00
	#define AMC_LINK_TYPE_EXT_STORAGE_SATA	0x01
	#define AMC_LINK_TYPE_EXT_STORAGE_SAS	0x02
#ifndef WORDS_BIGENDIAN
	unsigned short channel_id          :8;
	unsigned short port_flag_0         :1;
	unsigned short port_flag_1         :1;
	unsigned short port_flag_2         :1;
	unsigned short port_flag_3         :1;
	unsigned short type                :8;
	unsigned short type_ext            :4;
	unsigned short group_id            :8;
	unsigned char  asym_match          :2;
	unsigned char  /* reserved */      :6;
#else	
	unsigned char  /* reserved */      :6;
	unsigned char  asym_match          :2;
	unsigned short group_id            :8;
	unsigned short type_ext            :4;
	unsigned short type                :8;
	unsigned short port_flag_3         :1;
	unsigned short port_flag_2         :1;
	unsigned short port_flag_1         :1;
	unsigned short port_flag_0         :1;
	unsigned short channel_id          :8;
#endif 
} __attribute__ ((packed));
/* FRU Board manufacturing date */
static const uint64_t secs_from_1970_1996 = 820450800;
static const char * chassis_type_desc[] __attribute__((unused)) = {
	"Unspecified", "Other", "Unknown",
	"Desktop", "Low Profile Desktop", "Pizza Box",
	"Mini Tower", "Tower",
	    "Portable", "LapTop", "Notebook", "Hand Held",
	    "Docking Station", "All in One", "Sub Notebook",
	    "Space-saving", "Lunch Box", "Main Server Chassis",
	    "Expansion Chassis", "SubChassis", "Bus Expansion Chassis",
	    "Peripheral Chassis", "RAID Chassis", "Rack Mount Chassis"};

int ipmi_fru_main(struct ipmi_intf *intf, int argc, char **argv);
int ipmi_fru_print(struct ipmi_intf *intf, struct sdr_record_fru_locator *fru);

#endif /* IPMI_FRU_H */
