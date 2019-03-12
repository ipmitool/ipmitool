/****************************************************************************
Copyright (c) 2008, Dell Inc
All rights reserved.
Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:
- Redistributions of source code must retain the above copyright notice,
this list of conditions and the following disclaimer.

- Redistributions in binary form must reproduce the above copyright notice,
this list of conditions and the following disclaimer in the documentation
and/or other materials provided with the distribution.
- Neither the name of Dell Inc nor the names of its contributors
may be used to endorse or promote products derived from this software
without specific prior written permission.
THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
POSSIBILITY OF SUCH DAMAGE.


*****************************************************************************/
#ifndef IPMI_DELLOEM_H
#define IPMI_DELLOEM_H

#if HAVE_CONFIG_H
# include <config.h>
#endif
#pragma pack(1)
#define MIN(a,b) ((a) < (b) ? (a) : (b))
#define MAX(a,b) ((a) > (b) ? (a) : (b))
/* Dell selector for LCD control - get and set unless specified */
#define IPMI_DELL_LCD_STRING_SELECTOR 0xC1 /* RW get/set the user string */
#define IPMI_DELL_LCD_CONFIG_SELECTOR 0xC2 /* RW set to user/default/none */
#define IPMI_DELL_LCD_GET_CAPS_SELECTOR 0xCF /* RO use when available*/
#define IPMI_DELL_LCD_STRINGEX_SELECTOR 0xD0 /* RW get/set the user string use first when available*/
#define IPMI_DELL_LCD_STATUS_SELECTOR 0xE7 /* LCD string when config set to default.*/
#define IPMI_DELL_PLATFORM_MODEL_NAME_SELECTOR 0xD1 /* LCD string when config set to default.*/
/* Dell defines for picking which string to use */
#define IPMI_DELL_LCD_CONFIG_USER_DEFINED 0x00 /* use string set by user*/
#define IPMI_DELL_LCD_CONFIG_DEFAULT 0x01 /* use platform model name*/
#define IPMI_DELL_LCD_CONFIG_NONE 0x02 /* blank*/
#define IPMI_DELL_LCD_iDRAC_IPV4ADRESS 0x04 /* use string set by user*/
#define IPMI_DELL_LCD_IDRAC_MAC_ADDRESS 0x08 /* use platform model name*/
#define IPMI_DELL_LCD_OS_SYSTEM_NAME 0x10 /* blank*/
#define IPMI_DELL_LCD_SERVICE_TAG 0x20  /* use string set by user*/
#define IPMI_DELL_LCD_iDRAC_IPV6ADRESS 0x40  /* use string set by user*/
#define IPMI_DELL_LCD_AMBEINT_TEMP 0x80  /* use platform model name*/
#define IPMI_DELL_LCD_SYSTEM_WATTS 0x100 /* blank*/
#define IPMI_DELL_LCD_ASSET_TAG 0x200
#define IPMI_DELL_LCD_ERROR_DISP_SEL 0x01  /* use platform model name*/
#define IPMI_DELL_LCD_ERROR_DISP_VERBOSE 0x02  /* blank*/
#define IPMI_DELL_IDRAC_VALIDATOR 0xDD
#define IPMI_DELL_POWER_CAP_STATUS 0xBA
#define IPMI_DELL_AVG_POWER_CONSMP_HST 0xEB
#define IPMI_DELL_PEAK_POWER_CONSMP_HST 0xEC
#define SYSTEM_BOARD_SYSTEM_LEVEL_SENSOR_NUM 0x98
#define IDRAC_11G 1
#define IDRAC_12G 2
#define IDRAC_13G 3
#define IDRAC_14G 4
#define LICENSE_NOT_SUPPORTED 0x6F // Return Error code for license
#define VFL_NOT_LICENSED 0x33
#define btuphr 0x01
#define watt 0x00
#define IPMI_DELL_POWER_CAP 0xEA
#define percent 0x03
#define COMMON_OFFSET 0x00 /*14g fru arch*/
#define INTERNAL_OFFSET 0x01
#define CHASSIS_OFFSET 0x02
#define BOARD_OFFSET 0x03
#define PRODUCT_OFFSET 0x04
#define OFFSETMULTIPLIER 0x08
#define BOARDLANGOFFSET 0x02
#define BOARDPRODUCTMANFLENOFFSET 0x06
#define PARAM_REV_OFFSET (uint8_t)0x1
#define VIRTUAL_MAC_OFFSET (uint8_t)0x1
#define LOM_MACTYPE_ETHERNET 0
#define LOM_MACTYPE_ISCSI 1
#define LOM_MACTYPE_RESERVED 3
#define LOM_ETHERNET_ENABLED 0
#define LOM_ETHERNET_DISABLED 1
#define LOM_ETHERNET_PLAYINGDEAD 2
#define LOM_ETHERNET_RESERVED 3
#define LOM_ACTIVE 1
#define LOM_INACTIVE 0
#define MACADDRESSLENGH 6
#define MAX_LOM 8
#define EMB_NIC_MAC_ADDRESS_11G (uint8_t)0xDA
#define EMB_NIC_MAC_ADDRESS_9G_10G (uint8_t)0xCB
#define IMC_IDRAC_10G (uint8_t)0x08
#define IMC_CMC (uint8_t)0x09
#define IMC_IDRAC_11G_MONOLITHIC (uint8_t)0x0A
#define IMC_IDRAC_11G_MODULAR (uint8_t)0x0B
#define IMC_UNUSED (uint8_t)0x0C
#define IMC_MASER_LITE_BMC (uint8_t)0x0D
#define IMC_MASER_LITE_NU (uint8_t)0x0E
#define IMC_IDRAC_12G_MONOLITHIC (uint8_t)0x10
#define IMC_IDRAC_12G_MODULAR (uint8_t)0x11
#define IMC_IDRAC_13G_MONOLITHIC (uint8_t)0x20
#define IMC_IDRAC_13G_MODULAR (uint8_t)0x21
#define IMC_IDRAC_13G_DCS (uint8_t)0x22
#define IMC_IDRAC_14G_MONOLITHIC (uint8_t)0x30
#define IMC_IDRAC_14G_MODULAR (uint8_t)0x31
#define IMC_IDRAC_14G_DCS (uint8_t)0x32
#define TRANSPORT_NETFN (uint8_t)0xc
#define GET_LAN_PARAM_CMD (uint8_t)0x02
#define MAC_ADDR_PARAM  (uint8_t)0x05
#define LAN_CHANNEL_NUMBER (uint8_t)0x01
#define IDRAC_NIC_NUMBER (uint8_t)0x8
#define TOTAL_N0_NICS_INDEX (uint8_t)0x1
#define SET_NIC_SELECTION_12G_CMD (uint8_t)0x28 // 12g supported
#define GET_NIC_SELECTION_12G_CMD (uint8_t)0x29
#define SET_NIC_SELECTION_CMD (uint8_t)0x24 // 11g supported
#define GET_NIC_SELECTION_CMD (uint8_t)0x25
#define GET_ACTIVE_NIC_CMD (uint8_t)0xc1
#define POWER_EFFICENCY_CMD (uint8_t)0xc0
#define SERVER_POWER_CONSUMPTION_CMD (uint8_t)0x8F
#define POWER_SUPPLY_INFO (uint8_t)0xb0
#define IPMI_ENTITY_ID_POWER_SUPPLY (uint8_t)0x0a
#define SENSOR_STATE_STR_SIZE (uint8_t)64
#define SENSOR_NAME_STR_SIZE (uint8_t)64
#define GET_PWRMGMT_INFO_CMD (uint8_t)0x9C
#define CLEAR_PWRMGMT_INFO_CMD (uint8_t)0x9D
#define GET_PWR_HEADROOM_CMD (uint8_t)0xBB
#define GET_PWR_CONSUMPTION_CMD (uint8_t)0xB3
#define GET_FRONT_PANEL_INFO_CMD (uint8_t)0xb5
#define TAG_LEN 64
#define MAX_POWER_FW_VERSION 8
#define IPMI_DELL_LCD_STRING_LENGTH_MAX 62 /* Valid for 9G. Glacier ??. */
#define IPMI_DELL_LCD_STRING1_SIZE 14
#define IPMI_DELL_LCD_STRINGN_SIZE 16
#define IPMI_DELL_SYSINFO_HOST_NAME 0x02
#define IPMI_DELL_SYSINFO_ASSET_TAG 0xC4
#define IPMI_DELL_SYSINFO_ASSET_TAG_LENGTH 64
#define IPMI_DELL_SYSINFO_SERVICE_TAG 0xC5
#define IPMI_DELL_SYSINFO_SERVICE_TAG_LENGTH 64
#define IPMI_DELL_SYSINFO_OS_NAME 0x03
#define IPMI_DELL_SYSINFO_BIOS_VERSION 0x01
#define IPMI_DELL_SYSINFO_OS_VERSION_NUMBER 0x0E4
#define BIB_TYPE_BIOS_VERSION 0x00 /* BIB field type definitions */
#define BIB_TYPE_SERVICE_TAG 0x01
#define BIB_TYPE_ASSET_TAG 0x02
#define BIB_TYPE_PDRODUCT_MODEL 0x1A
#define IPMI_GET_EXT_SD_CARD_INFO 0xA4 /* vflash subcommands */
#define IPMI_VAL1 0x01
#define IPMI_VAL2 0x02
#define IPMI_VAL3 0x04
#define IPMI_VAL4 0x08
#define IPMI_VAL5 0x10
#define IPMI_VAL6 0x20
#define IPMI_VAL7 0x40
#define IPMI_VAL8 0x80
#define IPMI_IDRAC_VER2 0x04
#define IPMI_IDRAC_VER1 0x01
#define REQ_MSG_LEN 4
#define FRU_SHIFT 8
#define FRU_VAL 6
#define FRU_OFFSET_66 0x66
#define FRU_OFFSET_67 0x67
#define FRU_OFFSET_68 0x68
#define FRU_OFFSET_69 0x69
#define FRU_OFFSET_6A 0x6A
#define FRU_OFFSET_6B 0x6B
#define SYSINFO_ID_CMD 0xbf

#define VIRTUAL_MAC_OFFSET_11G_AND_OLD (uint8_t)(0x2)
#define VIRTUAL_MAC_OFFSET_12G_AND_LATER (uint8_t)(0x1)

#define GET_COMMAND_ID (uint8_t)0x01
#define SET_COMMAND_ID (uint8_t)0x00

#define GET_MISCELLANEOUS_CMD (uint8_t)0xD0
#define GET_HWCAPABILITY_SUB_CMD (uint8_t)0x12


/* Not on all Dell servers. If there, use it.*/
typedef struct _tag_ipmi_dell_lcd_caps
{
       uint8_t parm_rev; /* 0x11 for ipmi 2.0 */
        uint8_t char_set; /* always 1 for printable ascii 0x20-0x7e */
   uint8_t number_lines; /* 0-4, 1 for 9g. 10g tbd */
   uint8_t max_chars[4]; /* 62 for triathlon, 0 if not present (glacier) */
                         /* [0] is max chars for line 1 */
} __attribute__ ((packed)) ipmi_dell_lcd_caps;


typedef struct _tag_ipmi_dell_lcd_string
{
     uint8_t parm_rev; /* 0x11 for IPMI 2.0 */
     uint8_t data_block_selector; /* 16-byte data block number to access, 0 based.*/
     union
     {
          struct
          {
                uint8_t encoding : 4; /* 0 is printable ASCII 7-bit */
                uint8_t length; /* 0 to max chars from lcd caps */
                uint8_t data[IPMI_DELL_LCD_STRING1_SIZE]; /* not zero terminated.  */
          }selector_0_string;
          uint8_t selector_n_data[IPMI_DELL_LCD_STRINGN_SIZE];
     }lcd_string;
} __attribute__ ((packed)) ipmi_dell_lcd_string;


/* Only found on servers with more than 1 line. Use if available. */
typedef struct _tag_ipmi_dell_lcd_stringex
{
      uint8_t parm_rev; /* 0x11 for IPMI 2.0 */
      uint8_t line_number; /* LCD line number 1 to 4 */
      uint8_t data_block_selector; /* 16-byte data block number to access, 0 based.*/
      union
      {
           struct
           {
                uint8_t encoding : 4; /* 0 is printable ASCII 7-bit */
                uint8_t length; /* 0 to max chars from lcd caps */
                uint8_t data[IPMI_DELL_LCD_STRING1_SIZE]; /* not zero terminated.  */
           } selector_0_string;
           uint8_t selector_n_data[IPMI_DELL_LCD_STRINGN_SIZE];
   } lcd_string;
} __attribute__ ((packed)) ipmi_dell_lcd_stringex;


typedef struct _lcd_status
{
      char parametersel;
      char vkvm_status;
      char lock_status;
      char resv1;
      char resv;
} __attribute__ ((packed)) lcd_status;


typedef struct _lcd_mode
{
    uint8_t parametersel;
    uint32_t lcdmode;
    uint16_t lcdquallifier;
    uint32_t capabilites;
    uint8_t error_display;
    uint8_t resv;
} __attribute__ ((packed)) lcd_mode_t;


typedef struct _lom_mac_address_type
{
     unsigned int bladslotnumber : 4;
     unsigned int mactype : 2;
     unsigned int ethernetstatus : 2;
     unsigned int nicnumber : 5;
     unsigned int reserved : 3;
     uint8_t macaddressbyte[MACADDRESSLENGH];
}__attribute__ ((packed)) lom_mac_address_type;


typedef struct _embedded_nic_mac_address_type
{
     lom_mac_address_type lom_mac_address[MAX_LOM];
} __attribute__ ((packed)) embedded_nic_mac_address_type;


typedef struct _mac_address_type
{
     uint8_t mac_address_byte[MACADDRESSLENGH];

} __attribute__ ((packed)) mac_address_type;


typedef struct _embedded_nic_mac_address_type_10g
{
	mac_address_type mac_address [MAX_LOM];
}  __attribute__ ((packed)) embedded_nic_mac_address_type_10g;


typedef struct _tag_ipmi_dell_asset_tag
{
        uint8_t parm_rev;                                       /* 0x11 for IPMI 2.0 */
        uint8_t length;
        uint8_t tag[TAG_LEN];
} __attribute__ ((packed))  ipmi_dell_tag;


typedef struct _ipmi_power_monitor
{
    uint32_t        cumstarttime;
    uint32_t        cumreading;
    uint32_t        maxpeakstarttime;
    uint32_t        amppeaktime;
    uint16_t        ampreading;
    uint32_t        wattpeaktime;
    uint16_t        wattreading;
} __attribute__ ((packed)) ipmi_power_monitor;


typedef struct _ipmi_power_supply_infoo
{
	/*No param_rev it is not a System Information Command */
	uint16_t ratedWatts;
	uint16_t ratedAmps;
	uint16_t ratedVolts;
	uint32_t vendorid;
	uint8_t frimwareversion[MAX_POWER_FW_VERSION];
	uint8_t  powersupplytype;
	uint16_t rateddcwatts;
	uint16_t resv;
} __attribute__ ((packed)) ipmi_power_supply_info;


typedef struct ipmi_power_consumption_data
{
    uint16_t actualpowerconsumption;
    uint16_t powerthreshold;
    uint16_t warningthreshold;
    uint8_t throttlestate;
    uint16_t maxpowerconsumption;
    uint16_t throttlepowerconsumption;
    uint16_t resv;
} __attribute__ ((packed)) ipmi_power_consumption_data;


typedef struct ipmi_inst_power_consumption_data
{
    uint16_t instanpowerconsumption;
    uint16_t instanApms;
    uint16_t resv1;
    uint8_t resv;
} __attribute__ ((packed)) ipmi_inst_power_consumption_data;

typedef struct _ipmi_avgpower_consump_histroy
{
    uint8_t parameterselector;
    uint16_t lastminutepower;
    uint16_t lasthourpower;
    uint16_t lastdaypower;
    uint16_t lastweakpower;

} __attribute__ ((packed)) ipmi_avgpower_consump_history;

typedef struct _ipmi_power_consump_histroy
{
    uint8_t parameterselector;
    uint16_t lastminutepower;
    uint16_t lasthourpower;
    uint16_t lastdaypower;
    uint16_t lastweakpower;
    uint32_t lastminutepowertime;
    uint32_t lasthourpowertime;
    uint32_t lastdaypowertime;
    uint32_t lastweekpowertime;
} __attribute__ ((packed)) ipmi_power_consump_history;


typedef struct _ipmi_delloem_power_cap
{
    uint8_t parameterselector;
    uint16_t powercap;
    uint8_t unit;
    uint16_t maximumpowerconsmp;
    uint16_t minimumpowerconsmp;
    uint16_t totalnumpowersupp;
    uint16_t availablepower ;
    uint16_t systemthrottling;
    uint16_t resv;
} __attribute__ ((packed)) ipmi_power_cap;

typedef struct _power_headroom
{
    uint16_t instheadroom;
    uint16_t peakheadroom;
} __attribute__ ((packed)) power_headroom;

struct vflashstr {
        uint8_t val;
        const char * str;
};
typedef struct ipmi_vflash_extended_info
{
        uint8_t  vflashcompcode;
        uint8_t  sdcardstatus;
        uint32_t sdcardsize;
        uint32_t sdcardavailsize;
        uint8_t  bootpartion;
        uint8_t  resv;
} __attribute__ ((packed)) ipmi_dell_sdcard_info;


typedef struct _sensorreadingtype
{
    uint8_t sensorreading;
    uint8_t sensorflags;
    uint16_t sensorstate;
}sensorreadingtype;
uint16_t compareinputwattage(ipmi_power_supply_info* powersupplyinfo, uint16_t inputwattage);
int ipmi_delloem_main(struct ipmi_intf * intf, int argc, char ** argv);

#endif /*IPMI_DELLOEM_H*/

