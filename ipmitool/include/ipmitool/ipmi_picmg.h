
/*
	(C) Kontron
 */

#ifndef _IPMI_PICMG_H_
#define _IPMI_PICMG_H_

#include <ipmitool/ipmi.h>

/* PICMG commands */
#define PICMG_GET_PICMG_PROPERTIES_CMD             0x00
#define PICMG_GET_ADDRESS_INFO_CMD                 0x01
#define PICMG_GET_SHELF_ADDRESS_INFO_CMD           0x02
#define PICMG_SET_SHELF_ADDRESS_INFO_CMD           0x03
#define PICMG_FRU_CONTROL_CMD                      0x04
#define PICMG_GET_FRU_LED_PROPERTIES_CMD           0x05
#define PICMG_GET_LED_COLOR_CAPABILITIES_CMD       0x06
#define PICMG_SET_FRU_LED_STATE_CMD                0x07
#define PICMG_GET_FRU_LED_STATE_CMD                0x08
#define PICMG_SET_IPMB_CMD                         0x09
#define PICMG_SET_FRU_POLICY_CMD                   0x0A
#define PICMG_GET_FRU_POLICY_CMD                   0x0B
#define PICMG_FRU_ACTIVATION_CMD                   0x0C
#define PICMG_GET_DEVICE_LOCATOR_RECORD_CMD        0x0D
#define PICMG_SET_PORT_STATE_CMD                   0x0E
#define PICMG_GET_PORT_STATE_CMD                   0x0F
#define PICMG_COMPUTE_POWER_PROPERTIES_CMD         0x10
#define PICMG_SET_POWER_LEVEL_CMD                  0x11
#define PICMG_GET_POWER_LEVEL_CMD                  0x12
#define PICMG_RENEGOTIATE_POWER_CMD                0x13
#define PICMG_GET_FAN_SPEED_PROPERTIES_CMD         0x14
#define PICMG_SET_FAN_LEVEL_CMD                    0x15
#define PICMG_GET_FAN_LEVEL_CMD                    0x16
#define PICMG_BUSED_RESOURCE_CMD                   0x17

/* AMC.0 commands */
#define PICMG_AMC_SET_PORT_STATE_CMD			0x19
#define PICMG_AMC_GET_PORT_STATE_CMD			0x1A
/* AMC.0 R2.0 commands */
#define PICMG_AMC_SET_CLK_STATE_CMD				0x2C
#define PICMG_AMC_GET_CLK_STATE_CMD				0x2D

/* Site Types */
#define PICMG_ATCA_BOARD                           0x00
#define PICMG_POWER_ENTRY                          0x01
#define PICMG_SHELF_FRU                            0x02
#define PICMG_DEDICATED_SHMC                       0x03
#define PICMG_FAN_TRAY                             0x04
#define PICMG_FAN_FILTER_TRAY                      0x05
#define PICMG_ALARM                                0x06
#define PICMG_AMC                                  0x07
#define PICMG_PMC                                  0x08
#define PICMG_RTM                                  0x09

struct picmg_set_fru_activation_cmd {
   unsigned char  picmg_id;      /* always 0*/
   unsigned char  fru_id;        /* threshold setting mask */
   unsigned char  fru_state;     /* fru activation/deactivation */
} __attribute__ ((packed));

/* the LED color capabilities */
static const char* led_color_str[] __attribute__((unused)) = {
   "reserved",
   "BLUE",
   "RED",
   "GREEN",
   "AMBER",
   "ORANGE",
   "WHITE",
   "reserved"
};


static const char* amc_link_type_str[] __attribute__((unused)) = {
   "RESERVED",
   "RESERVED1",
   "PCI EXPRESS",
   "ADVANCED SWITCHING1",
   "ADVANCED SWITCHING2",
   "ETHERNET",
   "RAPIDIO",
   "STORAGE",
};

static const char* amc_link_type_ext_str[][16] __attribute__((unused)) = {
	/* FRU_PICMGEXT_AMC_LINK_TYPE_RESERVED */
	{
		"", "", "", "", "", "", "", "",   "", "", "", "", "", "", "", ""
	},
	/* FRU_PICMGEXT_AMC_LINK_TYPE_RESERVED1 */
	{
		"", "", "", "", "", "", "", "",   "", "", "", "", "", "", "", ""
	},
	/* FRU_PICMGEXT_AMC_LINK_TYPE_PCI_EXPRESS */
	{
		"Gen 1 - NSSC",
		"Gen 1 - SSC",
		"Gen 2 - NSSC",
		"Gen 2 - SSC",
		"", "", "", "",
		"", "", "", "", 
		"", "", "", ""
	},
	/* FRU_PICMGEXT_AMC_LINK_TYPE_ADVANCED_SWITCHING1 */
	{
		"Gen 1 - NSSC",
		"Gen 1 - SSC",
		"Gen 2 - NSSC",
		"Gen 2 - SSC",
		"", "", "", "",
		"", "", "", "", 
		"", "", "", ""
	},
	/* FRU_PICMGEXT_AMC_LINK_TYPE_ADVANCED_SWITCHING2 */
	{
		"Gen 1 - NSSC",
		"Gen 1 - SSC",
		"Gen 2 - NSSC",
		"Gen 2 - SSC",
		"", "", "", "",
		"", "", "", "", 
		"", "", "", ""
	},
	/* FRU_PICMGEXT_AMC_LINK_TYPE_ETHERNET */
	{
   		"1000BASE-BX (SerDES Gigabit)",
   		"10GBASE-BX410 Gigabit XAUI",
   		"", "", 
   		"", "", "", "",
		"", "", "", "", 
		"", "", "", ""
	},
	/* FRU_PICMGEXT_AMC_LINK_TYPE_RAPIDIO */
	{
   		"1.25 Gbaud transmission rate",
   		"2.5 Gbaud transmission rate",
   		"3.125 Gbaud transmission rate",
   		"", "", "", "", "",
		"", "", "", "", "", "", "", ""
	},
	/* FRU_PICMGEXT_AMC_LINK_TYPE_STORAGE */
	{
   		"Fibre Channel", 
   		"Serial ATA", 
   		"Serial Attached SCSI",
   		"", "", "", "", "",
		"", "", "", "", "", "", "", ""
	}
};

struct sAmcPortState {
#ifndef WORDS_BIGENDIAN
   unsigned short lane0       :  1;
   unsigned short lane1       :  1;
   unsigned short lane2       :  1;
   unsigned short lane3       :  1;
   unsigned short type        :  8;
   unsigned short type_ext    :  4;
   unsigned char  group_id    :  8;
#else
   unsigned char  group_id    :  8;
   unsigned short type_ext    :  4;
   unsigned short type        :  8;
   unsigned short lane3       :  1;
   unsigned short lane2       :  1;
   unsigned short lane1       :  1;
   unsigned short lane0       :  1;
#endif

   unsigned char state;
};


int ipmi_picmg_main (struct ipmi_intf * intf, int argc, char ** argv);

#endif
