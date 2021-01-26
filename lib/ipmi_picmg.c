/*
  Copyright (c) Kontron. All right reserved

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
 * Neither the name of Kontron, or the names of
 * contributors may be used to endorse or promote products derived
 * from this software without specific prior written permission.
 *
 * This software is provided "AS IS," without a warranty of any kind.
 * ALL EXPRESS OR IMPLIED CONDITIONS, REPRESENTATIONS AND WARRANTIES,
 * INCLUDING ANY IMPLIED WARRANTY OF MERCHANTABILITY, FITNESS FOR A
 * PARTICULAR PURPOSE OR NON-INFRINGEMENT, ARE HEREBY EXCLUDED.
 * DELL COMPUTERS ("DELL") AND ITS LICENSORS SHALL NOT BE LIABLE
 * FOR ANY DAMAGES SUFFERED BY LICENSEE AS A RESULT OF USING, MODIFYING
 * OR DISTRIBUTING THIS SOFTWARE OR ITS DERIVATIVES.  IN NO EVENT WILL
 * DELL OR ITS LICENSORS BE LIABLE FOR ANY LOST REVENUE, PROFIT OR DATA,
 * OR FOR DIRECT, INDIRECT, SPECIAL, CONSEQUENTIAL, INCIDENTAL OR
 * PUNITIVE DAMAGES, HOWEVER CAUSED AND REGARDLESS OF THE THEORY OF
 * LIABILITY, ARISING OUT OF THE USE OF OR INABILITY TO USE THIS SOFTWARE,
 * EVEN IF DELL HAS BEEN ADVISED OF THE POSSIBILITY OF SUCH DAMAGES.
 */


#include <ipmitool/ipmi_intf.h>
#include <ipmitool/ipmi_picmg.h>
#include <ipmitool/ipmi_fru.h>		/* for access to link descriptor defines */
#include <ipmitool/ipmi_strings.h>
#include <ipmitool/log.h>

#define PICMG_EKEY_MODE_QUERY          0
#define PICMG_EKEY_MODE_PRINT_ALL      1
#define PICMG_EKEY_MODE_PRINT_ENABLED  2
#define PICMG_EKEY_MODE_PRINT_DISABLED 3

#define PICMG_EKEY_MAX_CHANNEL          16
#define PICMG_EKEY_MAX_FABRIC_CHANNEL   15
#define PICMG_EKEY_MAX_INTERFACE 3

#define PICMG_EKEY_AMC_MAX_CHANNEL  16
#define PICMG_EKEY_AMC_MAX_DEVICE   15 /* 4 bits */


typedef enum picmg_bused_resource_mode {
	PICMG_BUSED_RESOURCE_SUMMARY,
} t_picmg_bused_resource_mode ;


typedef enum picmg_card_type {
	PICMG_CARD_TYPE_CPCI,
	PICMG_CARD_TYPE_ATCA,
	PICMG_CARD_TYPE_AMC,
	PICMG_CARD_TYPE_RESERVED
} t_picmg_card_type ;

static const char* amc_link_type_str[] = {
	"RESERVED",
	"RESERVED1",
	"PCI EXPRESS",
	"ADVANCED SWITCHING1",
	"ADVANCED SWITCHING2",
	"ETHERNET",
	"RAPIDIO",
	"STORAGE",
};

static const char* amc_link_type_ext_str[][16] = {
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

/* This is the version of the PICMG Extension */
static t_picmg_card_type PicmgCardType = PICMG_CARD_TYPE_RESERVED;

void
ipmi_picmg_help (void)
{
	lprintf(LOG_NOTICE, "PICMG commands:");
	lprintf(LOG_NOTICE, " properties           - get PICMG properties");
	lprintf(LOG_NOTICE, " frucontrol           - FRU control");
	lprintf(LOG_NOTICE, " addrinfo             - get address information");
	lprintf(LOG_NOTICE, " activate             - activate a FRU");
	lprintf(LOG_NOTICE, " deactivate           - deactivate a FRU");
	lprintf(LOG_NOTICE, " policy get           - get the FRU activation policy");
	lprintf(LOG_NOTICE, " policy set           - set the FRU activation policy");
	lprintf(LOG_NOTICE, " portstate get        - get port state");
	lprintf(LOG_NOTICE,
			" portstate getdenied  - get all denied[disabled] port description");
	lprintf(LOG_NOTICE,
			" portstate getgranted - get all granted[enabled] port description");
	lprintf(LOG_NOTICE,
			" portstate getall     - get all port state description");
	lprintf(LOG_NOTICE, " portstate set        - set port state");
	lprintf(LOG_NOTICE, " amcportstate get     - get port state");
	lprintf(LOG_NOTICE, " amcportstate set     - set port state");
	lprintf(LOG_NOTICE, " led prop             - get led properties");
	lprintf(LOG_NOTICE, " led cap              - get led color capabilities");
	lprintf(LOG_NOTICE, " led get              - get led state");
	lprintf(LOG_NOTICE, " led set              - set led state");
	lprintf(LOG_NOTICE, " power get            - get power level info");
	lprintf(LOG_NOTICE, " power set            - set power level");
	lprintf(LOG_NOTICE, " clk get              - get clk state");
	lprintf(LOG_NOTICE,
			" clk getdenied        - get all(up to 16) denied[disabled] clock descriptions");
	lprintf(LOG_NOTICE,
			" clk getgranted       - get all(up to 16) granted[enabled] clock descriptions");
	lprintf(LOG_NOTICE,
			" clk getall           - get all(up to 16) clock descriptions");
	lprintf(LOG_NOTICE, " clk set              - set clk state");
	lprintf(LOG_NOTICE,
			" busres summary       - display brief bused resource status info");
}


struct sAmcAddrMap {
	unsigned char ipmbLAddr;
	char*         amcBayId;
	unsigned char siteNum;
} amcAddrMap[] = {
	{0xFF, "reserved", 0},
	{0x72, "A1"      , 1},
	{0x74, "A2"      , 2},
	{0x76, "A3"      , 3},
	{0x78, "A4"      , 4},
	{0x7A, "B1"      , 5},
	{0x7C, "B2"      , 6},
	{0x7E, "B3"      , 7},
	{0x80, "B4"      , 8},
	{0x82, "reserved", 0},
	{0x84, "reserved", 0},
	{0x86, "reserved", 0},
	{0x88, "reserved", 0},
};

/* the LED color capabilities */
static const char *led_color_str[] = {
   "reserved",
   "BLUE",
   "RED",
   "GREEN",
   "AMBER",
   "ORANGE",
   "WHITE",
   "reserved"
};

const char *
picmg_led_color_str(int color)
{
	if (color < 0 || (size_t)color >= ARRAY_SIZE(led_color_str)) {
		return "invalid";
	}

	return led_color_str[color];
}

/* is_amc_channel - wrapper to convert user input into integer
 * AMC Channel range seems to be <0..255>, bits [7:0]
 *
 * @argv_ptr: source string to convert from; usually argv
 * @amc_chan_ptr: pointer where to store result
 * returns: zero on success, other values mean error
 */
int
is_amc_channel(const char *argv_ptr, uint8_t *amc_chan_ptr)
{
	if (!argv_ptr || !amc_chan_ptr) {
		lprintf(LOG_ERR, "is_amc_channel(): invalid argument(s).");
		return (-1);
	}
	if (str2uchar(argv_ptr, amc_chan_ptr) == 0) {
		return 0;
	}
	lprintf(LOG_ERR, "Given AMC Channel '%s' is invalid.", argv_ptr);
	return (-1);
}
/* is_amc_dev - wrapper to convert user input into integer.
 * AMC Dev ID limits are unknown.
 *
 * @argv_ptr: source string to convert from; usually argv
 * @amc_dev_ptr: pointer where to store result
 * returns: zero on success, other values mean error
 */
int
is_amc_dev(const char *argv_ptr, int32_t *amc_dev_ptr)
{
	if (!argv_ptr || !amc_dev_ptr) {
		lprintf(LOG_ERR, "is_amc_dev(): invalid argument(s).");
		return (-1);
	}
	if (str2int(argv_ptr, amc_dev_ptr) == 0 && *amc_dev_ptr >= 0) {
		return 0;
	}
	lprintf(LOG_ERR, "Given PICMG Device '%s' is invalid.",
			argv_ptr);
	return (-1);
}
/* is_amc_intf - wrapper to convert user input into integer.
 * AMC Interface (ID) limits are unknown.
 *
 * @argv_ptr: source string to convert from; usually argv
 * @amc_intf_ptr: pointer where to store result
 * returns: zero on success, other values mean error
 */
int
is_amc_intf(const char *argv_ptr, int32_t *amc_intf_ptr)
{
	if (!argv_ptr || !amc_intf_ptr) {
		lprintf(LOG_ERR, "is_amc_intf(): invalid argument(s).");
		return (-1);
	}
	if (str2int(argv_ptr, amc_intf_ptr) == 0 && *amc_intf_ptr >= 0) {
		return 0;
	}
	lprintf(LOG_ERR, "Given PICMG Interface '%s' is invalid.",
			argv_ptr);
	return (-1);
}
/* is_amc_port - wrapper to convert user input into integer.
 * AMC Port limits are unknown.
 *
 * @argv_ptr: source string to convert from; usually argv
 * @amc_port_ptr: pointer where to store result
 * returns: zero on success, other values mean error
 */
int
is_amc_port(const char *argv_ptr, int32_t *amc_port_ptr)
{
	if (!argv_ptr || !amc_port_ptr) {
		lprintf(LOG_ERR, "is_amc_port(): invalid argument(s).");
		return (-1);
	}
	if (str2int(argv_ptr, amc_port_ptr) == 0 && *amc_port_ptr >= 0) {
		return 0;
	}
	lprintf(LOG_ERR, "Given PICMG Port '%s' is invalid.", argv_ptr);
	return (-1);
}
/* is_clk_acc - wrapper to convert user input into integer.
 * Clock Accuracy limits are unknown[1byte by spec].
 *
 * @argv_ptr: source string to convert from; usually argv
 * @clk_acc_ptr: pointer where to store result
 * returns: zero on success, other values mean error
 */
int
is_clk_acc(const char *argv_ptr, uint8_t *clk_acc_ptr)
{
	if (!argv_ptr || !clk_acc_ptr) {
		lprintf(LOG_ERR, "is_clk_acc(): invalid argument(s).");
		return (-1);
	}
	if (str2uchar(argv_ptr, clk_acc_ptr) == 0) {
		return 0;
	}
	lprintf(LOG_ERR, "Given Clock Accuracy '%s' is invalid.",
			argv_ptr);
	return (-1);
}
/* is_clk_family - wrapper to convert user input into integer.
 * Clock Family limits are unknown[1byte by spec].
 *
 * @argv_ptr: source string to convert from; usually argv
 * @clk_family_ptr: pointer where to store result
 * returns: zero on success, other values mean error
 */
int
is_clk_family(const char *argv_ptr, uint8_t *clk_family_ptr)
{
	if (!argv_ptr || !clk_family_ptr) {
		lprintf(LOG_ERR, "is_clk_family(): invalid argument(s).");
		return (-1);
	}
	if (str2uchar(argv_ptr, clk_family_ptr) == 0) {
		return 0;
	}
	lprintf(LOG_ERR, "Given Clock Family '%s' is invalid.",
			argv_ptr);
	return (-1);
}
/* is_clk_freq - wrapper to convert user input into integer.
 * Clock Frequency limits are unknown, but specification says
 * 3Bytes + 1B checksum
 *
 * @argv_ptr: source string to convert from; usually argv
 * @clk_freq_ptr: pointer where to store result
 * returns: zero on success, other values mean error
 */
int
is_clk_freq(const char *argv_ptr, uint32_t *clk_freq_ptr)
{
	if (!argv_ptr || !clk_freq_ptr) {
		lprintf(LOG_ERR, "is_clk_freq(): invalid argument(s).");
		return (-1);
	}
	if (str2uint(argv_ptr, clk_freq_ptr) == 0) {
		return 0;
	}
	lprintf(LOG_ERR, "Given Clock Frequency '%s' is invalid.",
			argv_ptr);
	return (-1);
}
/* is_clk_id - wrapper to convert user input into integer.
 * Clock ID limits are unknown, however it's 1B by specification and I've
 * found two ranges: <1..5> or <0..15>
 *
 * @argv_ptr: source string to convert from; usually argv
 * @clk_id_ptr: pointer where to store result
 * returns: zero on success, other values mean error
 */
int
is_clk_id(const char *argv_ptr, uint8_t *clk_id_ptr)
{
	if (!argv_ptr || !clk_id_ptr) {
		lprintf(LOG_ERR, "is_clk_id(): invalid argument(s).");
		return (-1);
	}
	if (str2uchar(argv_ptr, clk_id_ptr) == 0) {
		return 0;
	}
	lprintf(LOG_ERR, "Given Clock ID '%s' is invalid.", argv_ptr);
	return (-1);
}
/* is_clk_index - wrapper to convert user input into integer.
 * Clock Index limits are unknown[1B by spec]
 *
 * @argv_ptr: source string to convert from; usually argv
 * @clk_index_ptr: pointer where to store result
 * returns: zero on success, other values mean error
 */
int
is_clk_index(const char *argv_ptr, uint8_t *clk_index_ptr)
{
	if (!argv_ptr || !clk_index_ptr) {
		lprintf(LOG_ERR, "is_clk_index(): invalid argument(s).");
		return (-1);
	}
	if (str2uchar(argv_ptr, clk_index_ptr) == 0) {
		return 0;
	}
	lprintf(LOG_ERR, "Given Clock Index '%s' is invalid.", argv_ptr);
	return (-1);
}
/* is_clk_resid - wrapper to convert user input into integer.
 * Clock Resource Index(?) limits are unknown, but maximum seems to be 15.
 *
 * @argv_ptr: source string to convert from; usually argv
 * @clk_resid_ptr: pointer where to store result
 * returns: zero on success, other values mean error
 */
int
is_clk_resid(const char *argv_ptr, int8_t *clk_resid_ptr)
{
	if (!argv_ptr || !clk_resid_ptr) {
		lprintf(LOG_ERR, "is_clk_resid(): invalid argument(s).");
		return (-1);
	}
	if (str2char(argv_ptr, clk_resid_ptr) == 0
			&& *clk_resid_ptr > (-1)) {
		return 0;
	}
	lprintf(LOG_ERR, "Given Resource ID '%s' is invalid.",
			clk_resid_ptr);
	return (-1);
}
/* is_clk_setting - wrapper to convert user input into integer.
 * Clock Setting is a 1B bitfield:
 * x [7:4] - reserved
 * x [3] - state - 0/1
 * x [2] - direction - 0/1
 * x [1:0] - PLL ctrl - 00/01/10/11[Reserved]
 *
 * @argv_ptr: source string to convert from; usually argv
 * @clk_setting_ptr: pointer where to store result
 * returns: zero on success, other values mean error
 */
int
is_clk_setting(const char *argv_ptr, uint8_t *clk_setting_ptr)
{
	if (!argv_ptr || !clk_setting_ptr) {
		lprintf(LOG_ERR, "is_clk_setting(): invalid argument(s).");
		return (-1);
	}
	if (str2uchar(argv_ptr, clk_setting_ptr) == 0) {
		return 0;
	}
	/* FIXME - validate bits 4-7 are 0 ? */
	lprintf(LOG_ERR, "Given Clock Setting '%s' is invalid.", argv_ptr);
	return (-1);
}
/* is_enable - wrapper to convert user input into integer.
 * Valid input range for Enable is <0..1>.
 *
 * @argv_ptr: source string to convert from; usually argv
 * @enable_ptr: pointer where to store result
 * returns: zero on success, other values mean error
 */
int
is_enable(const char *argv_ptr, uint8_t *enable_ptr)
{
	if (!argv_ptr || !enable_ptr) {
		lprintf(LOG_ERR, "is_enable(): invalid argument(s).");
		return (-1);
	}
	if (str2uchar(argv_ptr, enable_ptr) == 0
			&& (*enable_ptr == 0 || *enable_ptr == 1)) {
		return 0;
	}
	lprintf(LOG_ERR, "Given Enable '%s' is invalid.", argv_ptr);
	return (-1);
}
/* is_enable - wrapper to convert user input into integer.
 * LED colors: 
 * - valid <1..6>, <0xE..0xF>
 * - reserved [0, 7]
 * - undefined <8..D>
 *
 * @argv_ptr: source string to convert from; usually argv
 * @enable_ptr: pointer where to store result
 * returns: zero on success, other values mean error
 */
int
is_led_color(const char *argv_ptr, uint8_t *led_color_ptr)
{
	if (!argv_ptr || !led_color_ptr) {
		lprintf(LOG_ERR, "is_led_color(): invalid argument(s).");
		return (-1);
	}
	if (str2uchar(argv_ptr, led_color_ptr) != 0) {
		lprintf(LOG_ERR, "Given LED Color '%s' is invalid.",
				argv_ptr);
		lprintf(LOG_ERR,
				"LED Color must be from ranges: <1..6>, <0xE..0xF>");
		return (-1);
	}
	if ((*led_color_ptr >= 1 && *led_color_ptr <= 6)
			|| (*led_color_ptr >= 0xE && *led_color_ptr <= 0xF)) {
		return 0;
	}
	lprintf(LOG_ERR, "Given LED Color '%s' is out of range.", argv_ptr);
	lprintf(LOG_ERR, "LED Color must be from ranges: <1..6>, <0xE..0xF>");
	return (-1);
}
/* is_led_function - wrapper to convert user input into integer.
 * LED functions, however, might differ by OEM:
 * - 0x00 - off override
 * - <0x01..0xFA> - blinking override
 * - 0xFB - lamp test state
 * - 0xFC - state restored to local ctrl state
 * - <0xFD..0xFE> - reserved
 * - 0xFF - on override
 *
 * @argv_ptr: source string to convert from; usually argv
 * @led_fn_ptr: pointer where to store result
 * returns: zero on success, other values mean error
 */
int
is_led_function(const char *argv_ptr, uint8_t *led_fn_ptr)
{
	if (!argv_ptr || !led_fn_ptr) {
		lprintf(LOG_ERR, "is_led_function(): invalid argument(s).");
		return (-1);
	}
	if (str2uchar(argv_ptr, led_fn_ptr) == 0
			&& (*led_fn_ptr < 0xFD || *led_fn_ptr > 0xFE)) {
		return 0;
	}
	lprintf(LOG_ERR, "Given LED Function '%s' is invalid.", argv_ptr);
	return (-1);
}
/* is_led_id - wrapper to convert user input into integer.
 * LED ID range seems to be <0..255>
 *
 * @argv_ptr: source string to convert from; usually argv
 * @led_id_ptr: pointer where to store result
 * returns: zero on success, other values mean error
 */
int
is_led_id(const char *argv_ptr, uint8_t *led_id_ptr)
{
	if (!argv_ptr || !led_id_ptr) {
		lprintf(LOG_ERR, "is_led_id(): invalid argument(s).");
		return (-1);
	}
	if (str2uchar(argv_ptr, led_id_ptr) == 0) {
		return 0;
	}
	lprintf(LOG_ERR, "Given LED ID '%s' is invalid.", argv_ptr);
	return (-1);
}
/* is_link_group - wrapper to convert user input into integer.
 * Link Grouping ID limis are unknown, bits [31:24] by spec.
 *
 * @argv_ptr: source string to convert from; usually argv
 * @link_grp_ptr: pointer where to store result
 * returns: zero on success, other values mean error
 */
int
is_link_group(const char *argv_ptr, uint8_t *link_grp_ptr)
{
	if (!argv_ptr || !link_grp_ptr) {
		lprintf(LOG_ERR, "is_link_group(): invalid argument(s).");
		return (-1);
	}
	if (str2uchar(argv_ptr, link_grp_ptr) == 0) {
		return 0;
	}
	lprintf(LOG_ERR, "Given Link Group '%s' is invalid.", argv_ptr);
	return (-1);
}
/* is_link_type - wrapper to convert user input into integer.
 * Link Type limits are unknown, bits [19:12]
 *
 * @argv_ptr: source string to convert from; usually argv
 * @link_type_ptr: pointer where to store result
 * returns: zero on success, other values mean error
 */
int
is_link_type(const char *argv_ptr, uint8_t *link_type_ptr)
{
	if (!argv_ptr || !link_type_ptr) {
		lprintf(LOG_ERR, "is_link_type(): invalid argument(s).");
		return (-1);
	}
	if (str2uchar(argv_ptr, link_type_ptr) == 0) {
		return 0;
	}
	lprintf(LOG_ERR, "Given Link Type '%s' is invalid.", argv_ptr);
	return (-1);
}
/* is_link_type_ext - wrapper to convert user input into integer.
 * Link Type Extension limits are unknown, bits [23:20] => <0..15> ?
 *
 * @argv_ptr: source string to convert from; usually argv
 * @link_type_ext_ptr: pointer where to store result
 * returns: zero on success, other values mean error
 */
int
is_link_type_ext(const char *argv_ptr, uint8_t *link_type_ext_ptr)
{
	if (!argv_ptr || !link_type_ext_ptr) {
		lprintf(LOG_ERR, "is_link_type_ext(): invalid argument(s).");
		return (-1);
	}
	if (str2uchar(argv_ptr, link_type_ext_ptr) != 0
			|| *link_type_ext_ptr > 15) {
		lprintf(LOG_ERR,
				"Given Link Type Extension '%s' is invalid.",
				argv_ptr);
		return (-1);
	}
	return 0;
}

int
ipmi_picmg_getaddr(struct ipmi_intf * intf, int argc, char ** argv)
{
	struct ipmi_rs * rsp;
	struct ipmi_rq req;
	unsigned char msg_data[5];

	memset(&req, 0, sizeof(req));
	req.msg.netfn = IPMI_NETFN_PICMG;
	req.msg.cmd = PICMG_GET_ADDRESS_INFO_CMD;
	req.msg.data = msg_data;
	req.msg.data_len = 2;
	msg_data[0] = 0;   /* picmg identifier */
	msg_data[1] = 0;   /* default fru id */

	if(argc > 0) {
		if (is_fru_id(argv[0], &msg_data[1]) != 0) {
			return (-1);
		}
	}

	rsp = intf->sendrecv(intf, &req);
	if (!rsp) {
		lprintf(LOG_ERR, "Error. No valid response received.");
		return (-1);
	} else if (rsp->ccode) {
		lprintf(LOG_ERR, "Error getting address information CC: 0x%02x",
				rsp->ccode);
		return (-1);
	}

	printf("Hardware Address : 0x%02x\n", rsp->data[1]);
	printf("IPMB-0 Address   : 0x%02x\n", rsp->data[2]);
	printf("FRU ID           : 0x%02x\n", rsp->data[4]);
	printf("Site ID          : 0x%02x\n", rsp->data[5]);

	printf("Site Type        : ");
	switch (rsp->data[6]) {
	case PICMG_ATCA_BOARD:
		printf("ATCA board\n");
		break;
	case PICMG_POWER_ENTRY:
		printf("Power Entry Module\n");
		break;
	case PICMG_SHELF_FRU:
		printf("Shelf FRU\n");
		break;
	case PICMG_DEDICATED_SHMC:
		printf("Dedicated Shelf Manager\n");
		break;
	case PICMG_FAN_TRAY:
		printf("Fan Tray\n");
		break;
	case PICMG_FAN_FILTER_TRAY:
		printf("Fan Filter Tray\n");
		break;
	case PICMG_ALARM:
		printf("Alarm module\n");
		break;
	case PICMG_AMC:
		printf("AMC");
		printf("  -> IPMB-L Address: 0x%02x\n", amcAddrMap[rsp->data[5]].ipmbLAddr);
		break;
	case PICMG_PMC:
		printf("PMC\n");
		break;
	 case PICMG_RTM:
		printf("RTM\n");
		break;
	default:
		if (rsp->data[6] >= 0xc0 && rsp->data[6] <= 0xcf) {
			printf("OEM\n");
		} else {
			printf("unknown\n");
		}
	}

	return 0;
}

int
ipmi_picmg_properties(struct ipmi_intf * intf, int show )
{
	unsigned char PicmgExtMajorVersion;
	struct ipmi_rs * rsp;
	struct ipmi_rq req;
	unsigned char msg_data;

	memset(&req, 0, sizeof(req));
	req.msg.netfn    = IPMI_NETFN_PICMG;
	req.msg.cmd      = PICMG_GET_PICMG_PROPERTIES_CMD;
	req.msg.data     = &msg_data;
	req.msg.data_len = 1;
	msg_data = 0;

	rsp = intf->sendrecv(intf, &req);
	if (!rsp  || rsp->ccode) {
		lprintf(LOG_ERR, "Error getting address information.");
		return -1;
	}

	if( show )
	{
		printf("PICMG identifier	: 0x%02x\n", rsp->data[0]);
		printf("PICMG Ext. Version : %i.%i\n",	 rsp->data[1]&0x0f,
															 (rsp->data[1]&0xf0) >> 4);
		printf("Max FRU Device ID	: 0x%02x\n", rsp->data[2]);
		printf("FRU Device ID		: 0x%02x\n", rsp->data[3]);
	}

   /* We cache the major extension version ...
      to know how to format some commands */
	PicmgExtMajorVersion = rsp->data[1]&0x0f;

	if( PicmgExtMajorVersion == PICMG_CPCI_MAJOR_VERSION  ) { 
		PicmgCardType = PICMG_CARD_TYPE_CPCI;
   }
	else if(  PicmgExtMajorVersion == PICMG_ATCA_MAJOR_VERSION) {
		PicmgCardType = PICMG_CARD_TYPE_ATCA;
   }
	else if(  PicmgExtMajorVersion == PICMG_AMC_MAJOR_VERSION) {
		PicmgCardType = PICMG_CARD_TYPE_AMC;
   }
    
	return 0;
}



#define PICMG_FRU_DEACTIVATE	(unsigned char) 0x00
#define PICMG_FRU_ACTIVATE	(unsigned char) 0x01

int
ipmi_picmg_fru_activation(struct ipmi_intf * intf, char ** argv, unsigned char state)
{
	struct ipmi_rs * rsp;
	struct ipmi_rq req;

	struct picmg_set_fru_activation_cmd cmd;

	memset(&req, 0, sizeof(req));
	req.msg.netfn    = IPMI_NETFN_PICMG;
	req.msg.cmd      = PICMG_FRU_ACTIVATION_CMD;
	req.msg.data     = (unsigned char*) &cmd;
	req.msg.data_len = 3;

	cmd.picmg_id  = 0;						/* PICMG identifier */
	if (is_fru_id(argv[0], &(cmd.fru_id)) != 0) {
		return (-1);
	}
	cmd.fru_state = state;

	rsp = intf->sendrecv(intf, &req);

	if (!rsp  || rsp->ccode) {
		lprintf(LOG_ERR, "Error activation/deactivation of FRU.");
		return -1;
	}
	if (rsp->data[0] != 0x00) {
		lprintf(LOG_ERR, "Error activation/deactivation of FRU.");
	}

	return 0;
}


int
ipmi_picmg_fru_activation_policy_get(struct ipmi_intf * intf, char ** argv)
{
	struct ipmi_rs * rsp;
	struct ipmi_rq req;

	unsigned char msg_data[4];

	memset(&req, 0, sizeof(req));
	req.msg.netfn    = IPMI_NETFN_PICMG;
	req.msg.cmd      = PICMG_GET_FRU_POLICY_CMD;
	req.msg.data     = msg_data;
	req.msg.data_len = 2;

	msg_data[0] = 0;								/* PICMG identifier */
	if (is_fru_id(argv[0], &msg_data[1]) != 0) {
		return (-1);
	}

	rsp = intf->sendrecv(intf, &req);

	if (!rsp) {
		lprintf(LOG_ERR, "No valid response received.");
		return -1;
	}
	if (rsp->ccode) {
		lprintf(LOG_ERR, "FRU activation policy get failed with CC code 0x%02x",
				rsp->ccode);
		return -1;
	}

	printf(" %s\n", ((rsp->data[1] & 0x01) == 0x01) ?
	                           "activation locked" : "activation not locked");
	printf(" %s\n", ((rsp->data[1] & 0x02) == 0x02) ?
	                            "deactivation locked" : "deactivation not locked");

	return 0;
}

int
ipmi_picmg_fru_activation_policy_set(struct ipmi_intf * intf, char ** argv)
{
	struct ipmi_rs * rsp;
	struct ipmi_rq req;

	unsigned char msg_data[4];

	memset(&req, 0, sizeof(req));
	req.msg.netfn    = IPMI_NETFN_PICMG;
	req.msg.cmd      = PICMG_SET_FRU_POLICY_CMD;
	req.msg.data     = msg_data;
	req.msg.data_len = 4;

	msg_data[0] = 0;								            /* PICMG identifier */
	if (is_fru_id(argv[0], &msg_data[1]) != 0) {
		return (-1);
	}
	if (str2uchar(argv[1], &msg_data[2]) != 0 || msg_data[2] > 3) {
		/* FRU Lock Mask */
		lprintf(LOG_ERR, "Given FRU Lock Mask '%s' is invalid.",
				argv[1]);
		return (-1);
	}
	if (str2uchar(argv[2], &msg_data[3]) != 0 || msg_data[3] > 3) {
		/* FRU Act Policy */
		lprintf(LOG_ERR,
				"Given FRU Activation Policy '%s' is invalid.",
				argv[2]);
		return (-1);
	}

	rsp = intf->sendrecv(intf, &req);

	if (!rsp) {
		lprintf(LOG_ERR, "No valid response received.");
		return -1;
	}

	if (rsp->ccode) {
		lprintf(LOG_ERR, "FRU activation policy set failed with CC code 0x%02x",
				rsp->ccode);
		return -1;
	}

	return 0;
}

#define PICMG_MAX_LINK_PER_CHANNEL 4

int
ipmi_picmg_portstate_get(struct ipmi_intf * intf, int32_t interface,
		uint8_t channel, int mode)
{
	struct ipmi_rs * rsp = NULL;
	struct ipmi_rq req;

	unsigned char msg_data[4];

	struct fru_picmgext_link_desc* d; /* descriptor pointer for rec. data */

	memset(&req, 0, sizeof(req));

	req.msg.netfn    = IPMI_NETFN_PICMG;
	req.msg.cmd      = PICMG_GET_PORT_STATE_CMD;
	req.msg.data     = msg_data;
	req.msg.data_len = 2;

	msg_data[0] = 0x00;						/* PICMG identifier */
	msg_data[1] = (interface & 0x3)<<6;	/* interface      */
	msg_data[1] |= (channel & 0x3F);	/* channel number */

	rsp = intf->sendrecv(intf, &req);

	if (!rsp) {
		lprintf(LOG_ERR, "No valid response received.");
		return -1;
	}

	if (rsp->ccode) {
		if( mode == PICMG_EKEY_MODE_QUERY ){
			lprintf(LOG_ERR, "FRU portstate get failed with CC code 0x%02x",
					rsp->ccode);
		}
		return -1;
	}

	if (rsp->data_len >= 6) {
		int index;
		/* add support for more than one link per channel */
		for(index=0;index<PICMG_MAX_LINK_PER_CHANNEL;index++){
			if( rsp->data_len > (1+ (index*5))){
				d = (struct fru_picmgext_link_desc *) &(rsp->data[1 + (index*5)]);

				if
				(
					mode == PICMG_EKEY_MODE_PRINT_ALL
					||
					mode == PICMG_EKEY_MODE_QUERY
					||
					(
						mode == PICMG_EKEY_MODE_PRINT_ENABLED
						&&
						rsp->data[5 + (index*5) ] == 0x01
					)
					||
					(
						mode == PICMG_EKEY_MODE_PRINT_DISABLED
						&&
						rsp->data[5 + (index*5) ] == 0x00
					)
				)
				{
					printf("      Link Grouping ID:     0x%02x\n", d->grouping);
					printf("      Link Type Extension:  0x%02x\n", d->ext);
					printf("      Link Type:            0x%02x  ", d->type);
					if (d->type == 0 || d->type == 0xff)
					{
						printf("Reserved %d\n",d->type);
					}
					else if (d->type >= 0x06 && d->type <= 0xef)
					{
						printf("Reserved\n");
					}
					else if (d->type >= 0xf0 && d->type <= 0xfe)
					{
						printf("OEM GUID Definition\n");
					}
					else
					{
						switch (d->type)
						{
							case FRU_PICMGEXT_LINK_TYPE_BASE:
								printf("PICMG 3.0 Base Interface 10/100/1000\n");
							break;
							case FRU_PICMGEXT_LINK_TYPE_FABRIC_ETHERNET:
								printf("PICMG 3.1 Ethernet Fabric Interface\n");
							break;
							case FRU_PICMGEXT_LINK_TYPE_FABRIC_INFINIBAND:
								printf("PICMG 3.2 Infiniband Fabric Interface\n");
							break;
							case FRU_PICMGEXT_LINK_TYPE_FABRIC_STAR:
								printf("PICMG 3.3 Star Fabric Interface\n");
							break;
							case  FRU_PICMGEXT_LINK_TYPE_PCIE:
								printf("PCI Express Fabric Interface\n");
							break;
							default:
							printf("Invalid\n");
							break;
						}
					}
					printf("      Link Designator: \n");
					printf("        Port Flag:          0x%02x\n", d->desig_port);
					printf("        Interface:          0x%02x - ", d->desig_if);
					switch (d->desig_if)
					{
						case FRU_PICMGEXT_DESIGN_IF_BASE:
							printf("Base Interface\n");
						break;
						case FRU_PICMGEXT_DESIGN_IF_FABRIC:
							printf("Fabric Interface\n");
						break;
						case FRU_PICMGEXT_DESIGN_IF_UPDATE_CHANNEL:
							printf("Update Channel\n");
						break;
						case FRU_PICMGEXT_DESIGN_IF_RESERVED:
							printf("Reserved\n");
						break;
						default:
							printf("Invalid");
						break;
					}
					printf("        Channel Number:     0x%02x\n", d->desig_channel);
					printf("      STATE:                %s\n",
							( rsp->data[5 +(index*5)] == 0x01) ?"enabled":"disabled");
					printf("\n");
				}
			}
		}
	}
	else
	{
		lprintf(LOG_ERR, "Unexpected answer, can't print result.");
	}

	return 0;
}


int
ipmi_picmg_portstate_set(struct ipmi_intf * intf, int32_t interface,
		uint8_t channel, int32_t port, uint8_t type,
		uint8_t typeext, uint8_t group, uint8_t enable)
{
	struct ipmi_rs * rsp;
	struct ipmi_rq req;

	unsigned char msg_data[6];

	memset(&req, 0, sizeof(req));

	req.msg.netfn    = IPMI_NETFN_PICMG;
	req.msg.cmd      = PICMG_SET_PORT_STATE_CMD;
	req.msg.data     = msg_data;
	req.msg.data_len = 6;

	msg_data[0] = 0x00;												/* PICMG identifier */
	msg_data[1] = (channel & 0x3f) | ((interface & 3) << 6);
	msg_data[2] = (port & 0xf) | ((type & 0xf) << 4);
	msg_data[3] = ((type >> 4) & 0xf) | ((typeext & 0xf) << 4);
	msg_data[4] = group & 0xff;
	msg_data[5] = (enable & 0x01); /* enable/disable */

	rsp = intf->sendrecv(intf, &req);

	if (!rsp) {
		lprintf(LOG_ERR, "No valid response received.");
		return -1;
	}

	if (rsp->ccode) {
		lprintf(LOG_ERR, "Picmg portstate set failed with CC code 0x%02x",
				rsp->ccode);
		return -1;
	}

	return 0;
}



/* AMC.0 commands */

#define PICMG_AMC_MAX_LINK_PER_CHANNEL 4

int
ipmi_picmg_amc_portstate_get(struct ipmi_intf * intf, int32_t device,
		uint8_t channel, int mode)
{
	struct ipmi_rs * rsp;
	struct ipmi_rq req;

	unsigned char msg_data[4];

	struct fru_picmgext_amc_link_info* d; /* descriptor pointer for rec. data */

	memset(&req, 0, sizeof(req));

	req.msg.netfn	  = IPMI_NETFN_PICMG;
	req.msg.cmd		  = PICMG_AMC_GET_PORT_STATE_CMD;
	req.msg.data	  = msg_data;

	/* FIXME : add check for AMC or carrier device */
	if(device == -1 || PicmgCardType != PICMG_CARD_TYPE_ATCA ){
		req.msg.data_len = 2;	/* for amc only channel */
	}else{
		req.msg.data_len = 3;	/* for carrier channel and device */
	}

	msg_data[0] = 0x00;						/* PICMG identifier */
	msg_data[1] = channel ;
	msg_data[2] = device ;


	rsp = intf->sendrecv(intf, &req);

	if (!rsp) {
		lprintf(LOG_ERR, "No valid response received.");
		return -1;
	}

	if (rsp->ccode) {
		if( mode == PICMG_EKEY_MODE_QUERY ){
			lprintf(LOG_ERR, "Amc portstate get failed with CC code 0x%02x",
					rsp->ccode);
		}
		return -1;
	}

	if (rsp->data_len >= 5) {
		int index;

		/* add support for more than one link per channel */
		for(index=0;index<PICMG_AMC_MAX_LINK_PER_CHANNEL;index++){

			if( rsp->data_len > (1+ (index*4))){
				unsigned char type;
				unsigned char ext;
				unsigned char grouping;
				unsigned char port;
				unsigned char enabled;
				d = (struct fru_picmgext_amc_link_info *)&(rsp->data[1 + (index*4)]);


				/* Removed endianness check here, probably not required
					as we don't use bitfields  */
				port = d->linkInfo[0] & 0x0F;
				type = ((d->linkInfo[0] & 0xF0) >> 4 )|(d->linkInfo[1] & 0x0F );
				ext  = ((d->linkInfo[1] & 0xF0) >> 4 );
				grouping = d->linkInfo[2];


				enabled =  rsp->data[4 + (index*4) ];

				if
				(
					mode == PICMG_EKEY_MODE_PRINT_ALL
					||
					mode == PICMG_EKEY_MODE_QUERY
					||
					(
						mode == PICMG_EKEY_MODE_PRINT_ENABLED
						&&
						enabled == 0x01
					)
					||
					(
						mode == PICMG_EKEY_MODE_PRINT_DISABLED
						&&
						enabled	== 0x00
					)
				)
				{
					if(device == -1 || PicmgCardType != PICMG_CARD_TYPE_ATCA ){
						printf("   Link device :         AMC\n");
					}else{
                  printf("   Link device :         0x%02x\n", device );
					}

					printf("   Link Grouping ID:     0x%02x\n", grouping);

					if (type == 0 || type == 1 ||type == 0xff)
					{
						printf("   Link Type Extension:  0x%02x\n", ext);
						printf("   Link Type:            Reserved\n");
					}
					else if (type >= 0xf0 && type <= 0xfe)
					{
						printf("   Link Type Extension:  0x%02x\n", ext);
						printf("   Link Type:            OEM GUID Definition\n");
					}
					else
					{
						if (type <= FRU_PICMGEXT_AMC_LINK_TYPE_STORAGE )
						{
							printf("   Link Type Extension:  %s\n",
                                      amc_link_type_ext_str[type][ext]);
							printf("   Link Type:            %s\n",
                                      amc_link_type_str[type]);
						}
						else{
							printf("   Link Type Extension:  0x%02x\n", ext);
							printf("   Link Type:            undefined\n");
						}
					}
					printf("   Link Designator: \n");
					printf("      Channel Number:    0x%02x\n", channel);
					printf("      Port Flag:         0x%02x\n", port );
					printf("   STATE:                %s\n",
                              ( enabled == 0x01 )?"enabled":"disabled");
					printf("\n");
				}
			}
		}
	}
	else
	{
		lprintf(LOG_NOTICE,"ipmi_picmg_amc_portstate_get"\
							"Unexpected answer, can't print result");
	}

	return 0;
}


int
ipmi_picmg_amc_portstate_set(struct ipmi_intf * intf, uint8_t channel,
		int32_t port, uint8_t type, uint8_t typeext,
		uint8_t group, uint8_t enable, int32_t device)
{
	struct ipmi_rs	 * rsp;
	struct ipmi_rq	 req;
	unsigned char	 msg_data[7];

	memset(&req, 0, sizeof(req));

	req.msg.netfn	  = IPMI_NETFN_PICMG;
	req.msg.cmd		  = PICMG_AMC_SET_PORT_STATE_CMD;
	req.msg.data	  = msg_data;

	msg_data[0]	 = 0x00;						 /* PICMG identifier*/
	msg_data[1]	 = channel;					 /* channel id */
	msg_data[2]	 = port & 0xF;				 /* port flags */
	msg_data[2] |= (type & 0x0F)<<4;		 /* type	 */
	msg_data[3]	 = (type & 0xF0)>>4;		 /* type */
	msg_data[3] |= (typeext & 0x0F)<<4;	 /* extension */
	msg_data[4]	 = (group & 0xFF);		 /* group */
	msg_data[5]	 = (enable & 0x01);		 /* state */
	req.msg.data_len = 6;

	/* device id - only for carrier needed */
	if (device >= 0) {
		msg_data[6]	 = device;
		req.msg.data_len = 7;
	}

	rsp = intf->sendrecv(intf, &req);

	if (!rsp) {
		lprintf(LOG_ERR, "No valid response received.");
		return -1;
	}

	if (rsp->ccode) {
		lprintf(LOG_ERR, "Amc portstate set failed with CC code 0x%02x",
				rsp->ccode);
		return -1;
	}

	return 0;
}


int
ipmi_picmg_get_led_properties(struct ipmi_intf * intf, char ** argv)
{
	struct ipmi_rs * rsp;
	struct ipmi_rq req;

	unsigned char msg_data[6];

	memset(&req, 0, sizeof(req));

	req.msg.netfn = IPMI_NETFN_PICMG;
	req.msg.cmd	  = PICMG_GET_FRU_LED_PROPERTIES_CMD;
	req.msg.data  = msg_data;
	req.msg.data_len = 2;

	msg_data[0] = 0x00;									/* PICMG identifier */
	if (is_fru_id(argv[0], &msg_data[1]) != 0) {
		return (-1);
	}

	rsp = intf->sendrecv(intf, &req);

	if (!rsp) {
		lprintf(LOG_ERR, "No valid response received.");
		return -1;
	}

	if (rsp->ccode) {
		lprintf(LOG_ERR, "LED get properties failed with CC code 0x%02x",
				rsp->ccode);
		return -1;
	}

	printf("General Status LED Properties:  0x%2x\n", rsp->data[1] );
	printf("App. Specific  LED Count:       0x%2x\n", rsp->data[2] );

	return 0;
}

int
ipmi_picmg_get_led_capabilities(struct ipmi_intf * intf, char ** argv)
{
	int i;
	struct ipmi_rs * rsp;
	struct ipmi_rq req;

	unsigned char msg_data[6];

	memset(&req, 0, sizeof(req));

	req.msg.netfn = IPMI_NETFN_PICMG;
	req.msg.cmd	  = PICMG_GET_LED_COLOR_CAPABILITIES_CMD;
	req.msg.data  = msg_data;
	req.msg.data_len = 3;

	msg_data[0] = 0x00;									/* PICMG identifier */
	if (is_fru_id(argv[0], &msg_data[1]) != 0
			|| is_led_id(argv[1], &msg_data[2]) != 0) {
		return (-1);
	}

	rsp = intf->sendrecv(intf, &req);

	if (!rsp) {
		lprintf(LOG_ERR, "No valid response received.");
		return -1;
	}

	if (rsp->ccode) {
		lprintf(LOG_ERR, "LED get capabilities failed with CC code 0x%02x",
				rsp->ccode);
		return -1;
	}

	printf("LED Color Capabilities: ");
	for ( i=0 ; i<8 ; i++ ) {
		if ( rsp->data[1] & (0x01 << i) ) {
			printf("%s, ", picmg_led_color_str(i));
		}
	}
	printf("\n");

	printf("Default LED Color in\n");
	printf("      LOCAL control:  %s\n", picmg_led_color_str(rsp->data[2]));
	printf("      OVERRIDE state: %s\n", picmg_led_color_str(rsp->data[3]));

	return 0;
}

int
ipmi_picmg_get_led_state(struct ipmi_intf * intf, char ** argv)
{
	struct ipmi_rs * rsp;
	struct ipmi_rq req;

	unsigned char msg_data[6];

	memset(&req, 0, sizeof(req));

	req.msg.netfn = IPMI_NETFN_PICMG;
	req.msg.cmd	  = PICMG_GET_FRU_LED_STATE_CMD;
	req.msg.data  = msg_data;
	req.msg.data_len = 3;

	msg_data[0] = 0x00;									/* PICMG identifier */
	if (is_fru_id(argv[0], &msg_data[1]) != 0
			|| is_led_id(argv[1], &msg_data[2]) != 0) {
		return (-1);
	}

	rsp = intf->sendrecv(intf, &req);

	if (!rsp) {
		lprintf(LOG_ERR, "No valid response received.");
		return -1;
	}

	if (rsp->ccode) {
		lprintf(LOG_ERR, "LED get state failed with CC code 0x%02x", rsp->ccode);
		return -1;
	}

	printf("LED states:						  %x	", rsp->data[1] );

	if (!(rsp->data[1] & 0x1)) {
		printf("[NO LOCAL CONTROL]\n");
		return 0;
	}

	printf("[LOCAL CONTROL");

	if (rsp->data[1] & 0x2) {
		printf("|OVERRIDE");
	}

	if (rsp->data[1] & 0x4) {
		printf("|LAMPTEST");
	}

	printf("]\n");

	printf("  Local Control function:     %x  ", rsp->data[2] );
	if (rsp->data[2] == 0x0) {
		printf("[OFF]\n");
	} else if (rsp->data[2] == 0xff) {
		printf("[ON]\n");
	} else {
		printf("[BLINKING]\n");
	}

	printf("  Local Control On-Duration:  %x\n", rsp->data[3] );
	printf("  Local Control Color:        %x  [%s]\n",
	       rsp->data[4],
	       picmg_led_color_str(rsp->data[4]));

	/* override state or lamp test */
	if (rsp->data[1] & 0x02) {
		printf("  Override function:     %x  ", rsp->data[5] );
		if (rsp->data[2] == 0x0) {
			printf("[OFF]\n");
		} else if (rsp->data[2] == 0xff) {
			printf("[ON]\n");
		} else {
			printf("[BLINKING]\n");
		}

		printf("  Override On-Duration:  %x\n", rsp->data[6] );
		printf("  Override Color:        %x  [%s]\n",
		       rsp->data[7],
		       picmg_led_color_str(rsp->data[7]));

	}

	if (rsp->data[1] & 0x04) {
		printf("  Lamp test duration:    %x\n", rsp->data[8] );
	}

	return 0;
}

int
ipmi_picmg_set_led_state(struct ipmi_intf * intf, char ** argv)
{
	struct ipmi_rs * rsp;
	struct ipmi_rq req;

	unsigned char msg_data[6];

	memset(&req, 0, sizeof(req));

	req.msg.netfn = IPMI_NETFN_PICMG;
	req.msg.cmd = PICMG_SET_FRU_LED_STATE_CMD;
	req.msg.data = msg_data;
	req.msg.data_len = 6;

	msg_data[0] = 0x00;									/* PICMG identifier */
	if (is_fru_id(argv[0], &msg_data[1]) != 0
			|| is_led_id(argv[1], &msg_data[2]) != 0
			|| is_led_function(argv[2], &msg_data[3]) != 0
			|| is_led_color(argv[4], &msg_data[5]) != 0) {
		return (-1);
	}

	/* Validating the LED duration is not as simple as the other arguments, as
	 * the range of valid durations depends on the LED function.  From the spec:
	 *
	 * ``On-duration: LED on-time in tens of milliseconds if (1 <= Byte 4 <= FAh)
	 * Lamp Test time in hundreds of milliseconds if (Byte 4 = FBh). Lamp Test
	 * time value must be less than 128. Other values when Byte 4 = FBh are
	 * reserved. Otherwise, this field is ignored and shall be set to 0h.''
	 *
	 * If we're doing a lamp test, then the allowed values are 0 -> 127.
	 * Otherwise, the allowed values are 0 -> 255.  However, if the function is
	 * not a lamp test (0xFB) and outside the range 0x01 -> 0xFA then the value
	 * should be set to 0.
	 *
	 * Start by checking we have a parameter.
	 */
	if (!argv[3]) {
		lprintf(LOG_ERR, "LED Duration: invalid argument(s).");
		return (-1);
	}
	/* Next check we have a number. */
	if (str2uchar(argv[3], &msg_data[4]) != 0) {
		lprintf(LOG_ERR, "Given LED Duration '%s' is invalid", argv[3]);
		return (-1);
	}
	/* If we have a lamp test, ensure it's not too long a duration. */
	if (msg_data[3] == 0xFB && msg_data[4] > 127) {
		lprintf(LOG_ERR, "Given LED Duration '%s' is invalid", argv[3]);
		return (-1);
	}
	/* If we're outside the range that allows durations, set the duration to 0.
	 * Warn the user that we're doing this.
	 */
	if (msg_data[4] != 0 && (msg_data[3] == 0 || msg_data[3] > 0xFB)) {
		lprintf(LOG_WARN, "Setting LED Duration '%s' to '0'", argv[3]);
		msg_data[4] = 0;
	}

	rsp = intf->sendrecv(intf, &req);

	if (!rsp) {
		lprintf(LOG_ERR, "No valid response received.");
		return -1;
	}

	if (rsp->ccode) {
		lprintf(LOG_ERR, "LED set state failed with CC code 0x%02x", rsp->ccode);
		return -1;
	}


	return 0;
}

int
ipmi_picmg_get_power_level(struct ipmi_intf * intf, char ** argv)
{
	int i;
	struct ipmi_rs * rsp;
	struct ipmi_rq req;

	unsigned char msg_data[6];

	memset(&req, 0, sizeof(req));

	req.msg.netfn = IPMI_NETFN_PICMG;
	req.msg.cmd	  = PICMG_GET_POWER_LEVEL_CMD;
	req.msg.data  = msg_data;
	req.msg.data_len = 3;

	msg_data[0] = 0x00;									/* PICMG identifier */
	if (is_fru_id(argv[0], &msg_data[1]) != 0) {
		return (-1);
	}
	/* PICMG Power Type - <0..3> */
	if (str2uchar(argv[1], &msg_data[2]) != 0 || msg_data[2] > 3) {
		lprintf(LOG_ERR, "Given Power Type '%s' is invalid",
				argv[1]);
		return (-1);
	}

	rsp = intf->sendrecv(intf, &req);

	if (!rsp) {
		lprintf(LOG_ERR, "No valid response received.");
		return -1;
	}

	if (rsp->ccode) {
		lprintf(LOG_ERR, "Power level get failed with CC code 0x%02x", rsp->ccode);
		return -1;
	}

	printf("Dynamic Power Configuration: %s\n", (rsp->data[1]&0x80)==0x80?"enabled":"disabled" );
	printf("Actual Power Level:          %i\n", (rsp->data[1] & 0xf));
	printf("Delay to stable Power:       %i\n", rsp->data[2]);
	printf("Power Multiplier:            %i\n", rsp->data[3]);


	for ( i = 1; i+3 < rsp->data_len ; i++ ) {
		printf("   Power Draw %i:            %i\n", i, (rsp->data[i+3]) * rsp->data[3] / 10);
	}
	return 0;
}

int
ipmi_picmg_set_power_level(struct ipmi_intf * intf, char ** argv)
{
	struct ipmi_rs * rsp;
	struct ipmi_rq req;

	unsigned char msg_data[6];

	memset(&req, 0, sizeof(req));

	req.msg.netfn = IPMI_NETFN_PICMG;
	req.msg.cmd	  = PICMG_SET_POWER_LEVEL_CMD;
	req.msg.data  = msg_data;
	req.msg.data_len = 4;

	msg_data[0] = 0x00;					/* PICMG identifier	 */
	if (is_fru_id(argv[0], &msg_data[1]) != 0) {
		return (-1);
	}
	/* PICMG Power Level - <0x00..0x14>, [0xFF] */
	if (str2uchar(argv[1], &msg_data[2]) != 0
			|| (msg_data[2] > 0x14 && msg_data[2] != 0xFF)) {
		lprintf(LOG_ERR,
				"Given PICMG Power Level '%s' is invalid.",
				argv[1]);
		return (-1);
	}
	/* PICMG Present-to-desired - <0..1> */
	if (str2uchar(argv[2], &msg_data[3]) != 0 || msg_data[3] > 1) {
		lprintf(LOG_ERR,
				"Given PICMG Present-to-desired '%s' is invalid.",
				argv[2]);
		return (-1);
	}

	rsp = intf->sendrecv(intf, &req);

	if (!rsp) {
		lprintf(LOG_ERR, "No valid response received.");
		return -1;
	}

	if (rsp->ccode) {
		lprintf(LOG_ERR, "Power level set failed with CC code 0x%02x", rsp->ccode);
		return -1;
	}

	return 0;
}

int
ipmi_picmg_bused_resource(struct ipmi_intf * intf, t_picmg_bused_resource_mode mode)
{
	struct ipmi_rs * rsp;
	struct ipmi_rq req;

	unsigned char msg_data[6];
	memset(&req, 0, sizeof(req));

   int status = 0;
   switch ( mode ) {
      case PICMG_BUSED_RESOURCE_SUMMARY:
      {
         t_picmg_busres_resource_id resource;
         t_picmg_busres_board_cmd_types cmd =PICMG_BUSRES_BOARD_CMD_QUERY;

         req.msg.netfn	  = IPMI_NETFN_PICMG;
         req.msg.cmd	     = PICMG_BUSED_RESOURCE_CMD;
         req.msg.data	  = msg_data;
         req.msg.data_len = 3;

         /* IF BOARD
            query for all resources
         */
         for( resource=PICMG_BUSRES_METAL_TEST_BUS_1;resource<=PICMG_BUSRES_SYNC_CLOCK_GROUP_3;resource+=(t_picmg_busres_resource_id)1 ) {
            msg_data[0] = 0x00;					/* PICMG identifier */
            msg_data[1] = (unsigned char) cmd;
            msg_data[2] = (unsigned char) resource;
            rsp = intf->sendrecv(intf, &req);

            if (!rsp) {
               printf("bused resource control: no response\n");
               return -1;
            }

            if (rsp->ccode) {
               printf("bused resource control: returned CC code 0x%02x\n", rsp->ccode);
               return -1;
            } else {
               printf("Resource 0x%02x '%-26s' : 0x%02x [%s] \n" , 
                       resource, val2str(resource,picmg_busres_id_vals),
                       rsp->data[1], oemval2str(cmd,rsp->data[1],
                      picmg_busres_board_status_vals));
            }
         }
      }
      break;
      default :
      break;
   }

   return status;
}

int
ipmi_picmg_fru_control(struct ipmi_intf * intf, char ** argv)
{
	struct ipmi_rs * rsp;
	struct ipmi_rq req;

	unsigned char msg_data[6];

	memset(&req, 0, sizeof(req));

	req.msg.netfn	  = IPMI_NETFN_PICMG;
	req.msg.cmd	  = PICMG_FRU_CONTROL_CMD;
	req.msg.data	  = msg_data;
	req.msg.data_len = 3;

	msg_data[0] = 0x00;					/* PICMG identifier */
	if (is_fru_id(argv[0], &msg_data[1]) != 0) {
		return (-1);
	}
	/* FRU Control Option, valid range: <0..4> */
	if (str2uchar(argv[1], &msg_data[2]) != 0 || msg_data[2] > 4) {
		lprintf(LOG_ERR,
				"Given FRU Control Option '%s' is invalid.",
				argv[1]);
		return (-1);
	}

	printf("FRU Device Id: %d FRU Control Option: %s\n", msg_data[1],  \
				val2str( msg_data[2], picmg_frucontrol_vals));

	rsp = intf->sendrecv(intf, &req);

	if (!rsp) {
		lprintf(LOG_ERR, "No valid response received.");
		return -1;
	}

	if (rsp->ccode) {
		lprintf(LOG_ERR, "frucontrol failed with CC code 0x%02x", rsp->ccode);
		return -1;
	} else {
      printf("frucontrol: ok\n");
	}



	return 0;
}


int
ipmi_picmg_clk_get(struct ipmi_intf * intf, uint8_t clk_id, int8_t clk_res,
		int mode)
{
	struct ipmi_rs * rsp;
	struct ipmi_rq req;

	unsigned char enabled;
	unsigned char direction;

	unsigned char msg_data[6];

	memset(&req, 0, sizeof(req));

	req.msg.netfn = IPMI_NETFN_PICMG;
	req.msg.cmd   = PICMG_AMC_GET_CLK_STATE_CMD;
	req.msg.data  = msg_data;

	msg_data[0] = 0x00;									/* PICMG identifier	 */
	msg_data[1] = clk_id;

	if(clk_res == -1 || PicmgCardType != PICMG_CARD_TYPE_ATCA ){
		req.msg.data_len = 2;	/* for amc only channel */
	}else{
		req.msg.data_len = 3;	/* for carrier channel and device */
      msg_data[2] = clk_res;
	}

	rsp = intf->sendrecv(intf, &req);

	if (!rsp) {
		lprintf(LOG_ERR, "No valid response received.");
		return -1;
	}

	if (rsp->ccode && (mode == PICMG_EKEY_MODE_QUERY) ) {
		lprintf(LOG_ERR, "Clk get failed with CC code 0x%02x", rsp->ccode);
		return -1;
	}

	if (!rsp->ccode) {
		enabled	 = (rsp->data[1]&0x8)!=0;
		direction = (rsp->data[1]&0x4)!=0;

		if
		( 
			mode == PICMG_EKEY_MODE_QUERY
 			||
 			mode == PICMG_EKEY_MODE_PRINT_ALL
 			||
 			(
 				mode == PICMG_EKEY_MODE_PRINT_DISABLED
 				&&
 				enabled == 0
 			)
 			||
 			(
 				mode == PICMG_EKEY_MODE_PRINT_ENABLED
 				&&
 				enabled == 1
         )	
		) {
			if( PicmgCardType != PICMG_CARD_TYPE_AMC ) {
				printf("CLK resource id   : %3d [ %s ]\n", clk_res ,
					oemval2str( ((clk_res>>6)&0x03), (clk_res&0x0F),
														picmg_clk_resource_vals));				
			} else {
				printf("CLK resource id   : N/A [ AMC Module ]\n");
				clk_res = 0x40; /* Set */
			} 
         printf("CLK id            : %3d [ %s ]\n", clk_id,
					oemval2str( ((clk_res>>6)&0x03), clk_id ,
														picmg_clk_id_vals));				


			printf("CLK setting       : 0x%02x\n", rsp->data[1]);
			printf(" - state:     %s\n", (enabled)?"enabled":"disabled");
			printf(" - direction: %s\n", (direction)?"Source":"Receiver");
			printf(" - PLL ctrl:  0x%x\n", rsp->data[1]&0x3);

		   if(enabled){
		      unsigned long freq = 0;
		      freq = (  rsp->data[5] <<  0
		              | rsp->data[6] <<  8
		              | rsp->data[7] << 16
		              | rsp->data[8] << 24 );
		      printf("  - Index:  %3d\n", rsp->data[2]);
		      printf("  - Family: %3d [ %s ] \n", rsp->data[3], 
						val2str( rsp->data[3], picmg_clk_family_vals));
		      printf("  - AccLVL: %3d [ %s ] \n", rsp->data[4], 
						oemval2str( rsp->data[3], rsp->data[4],
											picmg_clk_accuracy_vals));
		
		      printf("  - Freq:   %ld\n", freq);
		   }
		}
	}
	return 0;
}


int
ipmi_picmg_clk_set(struct ipmi_intf * intf, int argc, char ** argv)
{
	struct ipmi_rs * rsp;
	struct ipmi_rq req;

	unsigned char msg_data[11] = {0};
	uint32_t freq = 0;

	memset(&req, 0, sizeof(req));

	req.msg.netfn = IPMI_NETFN_PICMG;
	req.msg.cmd	  = PICMG_AMC_SET_CLK_STATE_CMD;
	req.msg.data  = msg_data;

	msg_data[0] = 0x00;									/* PICMG identifier	 */
	if (is_clk_id(argv[0], &msg_data[1]) != 0
			|| is_clk_index(argv[1], &msg_data[2]) != 0
			|| is_clk_setting(argv[2], &msg_data[3]) != 0
			|| is_clk_family(argv[3], &msg_data[4]) != 0
			|| is_clk_acc(argv[4], &msg_data[5]) != 0
			|| is_clk_freq(argv[5], &freq) != 0) {
		return (-1);
	}

	msg_data[6] = (freq >> 0)& 0xFF;		/* freq					 */
	msg_data[7] = (freq >> 8)& 0xFF;		/* freq					 */
	msg_data[8] = (freq >>16)& 0xFF;		/* freq					 */
	msg_data[9] = (freq >>24)& 0xFF;		/* freq					 */

	req.msg.data_len = 10;
   if( PicmgCardType == PICMG_CARD_TYPE_ATCA  )
   {
      if( argc > 7)
      {
         req.msg.data_len = 11;
		 if (is_clk_resid(argv[6], &msg_data[10]) != 0) {
			 return (-1);
		 }
      }
      else
      {
         lprintf(LOG_ERR, "Missing resource id for atca board.");
         return -1;
      }
   }


	rsp = intf->sendrecv(intf, &req);
	if (!rsp) {
		lprintf(LOG_ERR, "No valid response received.");
		return -1;
	}

	if (rsp->ccode) {
		lprintf(LOG_ERR, "Clk set failed with CC code 0x%02x", rsp->ccode);
		return -1;
	}

	return 0;
}



int
ipmi_picmg_main (struct ipmi_intf * intf, int argc, char ** argv)
{
	int rc = 0;
	int showProperties = 0;

	if (!argc || !strcmp(argv[0], "help")) {
		ipmi_picmg_help();
		return 0;
	}

	/* Get PICMG properties is called to obtain version information */
	if (!strcmp(argv[0], "properties")) {
		showProperties =1;
	}
	rc = ipmi_picmg_properties(intf,showProperties);

	/* address info command */
	if (!strcmp(argv[0], "addrinfo")) {
		rc = ipmi_picmg_getaddr(intf, argc-1, &argv[1]);
	}
	else if (!strcmp(argv[0], "busres")) {
		if (argc > 1) {
			if (!strcmp(argv[1], "summary")) {
				ipmi_picmg_bused_resource(intf, PICMG_BUSED_RESOURCE_SUMMARY );
			}
		} else {
			lprintf(LOG_NOTICE, "usage: busres summary");
      }
	}
	/* fru control command */
	else if (!strcmp(argv[0], "frucontrol")) {
		if (argc > 2) {
			rc = ipmi_picmg_fru_control(intf, &(argv[1]));
		}
		else {
			lprintf(LOG_NOTICE, "usage: frucontrol <FRU-ID> <OPTION>");
			lprintf(LOG_NOTICE, "   OPTION:");
			lprintf(LOG_NOTICE, "      0      - Cold Reset");
			lprintf(LOG_NOTICE, "      1      - Warm Reset");
			lprintf(LOG_NOTICE, "      2      - Graceful Reboot");
			lprintf(LOG_NOTICE, "      3      - Issue Diagnostic Interrupt");
			lprintf(LOG_NOTICE, "      4      - Quiesce [AMC only]");
			lprintf(LOG_NOTICE, "      5-255  - Reserved");

			return -1;
		}

	}

	/* fru activation command */
	else if (!strcmp(argv[0], "activate")) {
		if (argc > 1) {
			rc = ipmi_picmg_fru_activation(intf, &(argv[1]), PICMG_FRU_ACTIVATE);
		}
		else {
			lprintf(LOG_ERR, "Specify the FRU to activate.");
			return -1;
		}
	}

	/* fru deactivation command */
	else if (!strcmp(argv[0], "deactivate")) {
		if (argc > 1) {
			rc = ipmi_picmg_fru_activation(intf, &(argv[1]), PICMG_FRU_DEACTIVATE);
		}else {
			lprintf(LOG_ERR, "Specify the FRU to deactivate.");
			return -1;
		}
	}

	/* activation policy command */
	else if (!strcmp(argv[0], "policy")) {
		if (argc > 1) {
			if (!strcmp(argv[1], "get")) {
				if (argc > 2) {
					rc = ipmi_picmg_fru_activation_policy_get(intf, &(argv[2]));
				} else {
					lprintf(LOG_NOTICE, "usage: get <fruid>");
				}
			} else if (!strcmp(argv[1], "set")) {
				if (argc > 4) {
					rc = ipmi_picmg_fru_activation_policy_set(intf, &(argv[2]));
				} else {
					lprintf(LOG_NOTICE, "usage: set <fruid> <lockmask> <lock>");
					lprintf(LOG_NOTICE,
							"    lockmask:  [1] affect the deactivation locked bit");
					lprintf(LOG_NOTICE,
							"               [0] affect the activation locked bit");
					lprintf(LOG_NOTICE,
							"    lock:      [1] set/clear deactivation locked");
					lprintf(LOG_NOTICE, "               [0] set/clear locked");
				}
			}
			else {
				lprintf(LOG_ERR, "Specify FRU.");
				return -1;
			}
		} else {
			lprintf(LOG_ERR, "Wrong parameters.");
			return -1;
		}
	}

	/* portstate command */
	else if (!strcmp(argv[0], "portstate")) {

		lprintf(LOG_DEBUG,"PICMG: portstate API");

		if (argc > 1) {
			if (!strcmp(argv[1], "get")) {
				int32_t iface;
				uint8_t channel = 0;

				lprintf(LOG_DEBUG,"PICMG: get");

				if(!strcmp(argv[1], "getall")) {
					for(iface=0;iface<=PICMG_EKEY_MAX_INTERFACE;iface++) {
						for(channel=1;channel<=PICMG_EKEY_MAX_CHANNEL;channel++) {
							if(!(( iface == FRU_PICMGEXT_DESIGN_IF_FABRIC ) &&
							      ( channel > PICMG_EKEY_MAX_FABRIC_CHANNEL ) ))
							{
								rc = ipmi_picmg_portstate_get(intf,iface,channel,
								        PICMG_EKEY_MODE_PRINT_ALL);
							}
						}
					}
				}
				else if(!strcmp(argv[1], "getgranted")) {
					for(iface=0;iface<=PICMG_EKEY_MAX_INTERFACE;iface++) {
						for(channel=1;channel<=PICMG_EKEY_MAX_CHANNEL;channel++) {
							rc = ipmi_picmg_portstate_get(intf,iface,channel,
							            PICMG_EKEY_MODE_PRINT_ENABLED);
						}
					}
				}
				else if(!strcmp(argv[1], "getdenied")){
					for(iface=0;iface<=PICMG_EKEY_MAX_INTERFACE;iface++) {
						for(channel=1;channel<=PICMG_EKEY_MAX_CHANNEL;channel++) {
							rc = ipmi_picmg_portstate_get(intf,iface,channel,
							           PICMG_EKEY_MODE_PRINT_DISABLED);
						}
					}
				}
				else if (argc > 3){
					if (is_amc_intf(argv[2], &iface) != 0
							|| is_amc_channel(argv[3], &channel) != 0) {
						return (-1);
					}
					lprintf(LOG_DEBUG,"PICMG: requesting interface %d",iface);
					lprintf(LOG_DEBUG,"PICMG: requesting channel %d",channel);

					rc = ipmi_picmg_portstate_get(intf,iface,channel,
					            PICMG_EKEY_MODE_QUERY );
				}
				else {
					lprintf(LOG_NOTICE, "<intf> <chn>|getall|getgranted|getdenied");
				}
			}
			else if (!strcmp(argv[1], "set")) {
					if (argc == 9) {
						int32_t interface = 0;
						int32_t port = 0;
						uint8_t channel = 0;
						uint8_t enable = 0;
						uint8_t group = 0;
						uint8_t type = 0;
						uint8_t typeext = 0;
						if (is_amc_intf(argv[2], &interface) != 0
								|| is_amc_channel(argv[3], &channel) != 0
								|| is_amc_port(argv[4], &port) != 0
								|| is_link_type(argv[5], &type) != 0
								|| is_link_type_ext(argv[6], &typeext) != 0
								|| is_link_group(argv[7], &group) != 0
								|| is_enable(argv[8], &enable) != 0) {
							return (-1);
						}

						lprintf(LOG_DEBUG,"PICMG: interface %d",interface);
						lprintf(LOG_DEBUG,"PICMG: channel %d",channel);
						lprintf(LOG_DEBUG,"PICMG: port %d",port);
						lprintf(LOG_DEBUG,"PICMG: type %d",type);
						lprintf(LOG_DEBUG,"PICMG: typeext %d",typeext);
						lprintf(LOG_DEBUG,"PICMG: group %d",group);
						lprintf(LOG_DEBUG,"PICMG: enable %d",enable);

						rc = ipmi_picmg_portstate_set(intf, interface,
						    channel, port, type, typeext  ,group ,enable);
					}
					else {
						lprintf(LOG_NOTICE,
								"<intf> <chn> <port> <type> <ext> <group> <1|0>");
						return -1;
					}
			}
		}
		else {
			lprintf(LOG_NOTICE, "<set>|<getall>|<getgranted>|<getdenied>");
			return -1;
		}
	}
	/* amc portstate command */
	else if (!strcmp(argv[0], "amcportstate")) {

		lprintf(LOG_DEBUG,"PICMG: amcportstate API");

		if (argc > 1) {
			if (!strcmp(argv[1], "get")){
				int32_t device;
				uint8_t channel;

				lprintf(LOG_DEBUG,"PICMG: get");

				if(!strcmp(argv[1], "getall")){
					int maxDevice = PICMG_EKEY_AMC_MAX_DEVICE;
					if( PicmgCardType != PICMG_CARD_TYPE_ATCA ){
						maxDevice = 0;
					}
					for(device=0;device<=maxDevice;device++){
						for(channel=0;channel<=PICMG_EKEY_AMC_MAX_CHANNEL;channel++){
							rc = ipmi_picmg_amc_portstate_get(intf,device,channel,
																	PICMG_EKEY_MODE_PRINT_ALL);
						}
					}
				}
				else if(!strcmp(argv[1], "getgranted")){
					int maxDevice = PICMG_EKEY_AMC_MAX_DEVICE;
					if( PicmgCardType != PICMG_CARD_TYPE_ATCA ){
						maxDevice = 0;
					}
					for(device=0;device<=maxDevice;device++){
						for(channel=0;channel<=PICMG_EKEY_AMC_MAX_CHANNEL;channel++){
							rc = ipmi_picmg_amc_portstate_get(intf,device,channel,
																  PICMG_EKEY_MODE_PRINT_ENABLED);
						}
					}
				}
				else if(!strcmp(argv[1], "getdenied")){
					int maxDevice = PICMG_EKEY_AMC_MAX_DEVICE;
					if( PicmgCardType != PICMG_CARD_TYPE_ATCA ){
						maxDevice = 0;
					}
					for(device=0;device<=maxDevice;device++){
						for(channel=0;channel<=PICMG_EKEY_AMC_MAX_CHANNEL;channel++){
							rc = ipmi_picmg_amc_portstate_get(intf,device,channel,
                                                 PICMG_EKEY_MODE_PRINT_DISABLED);
						}
					}
				}
				else if (argc > 2){
					if (is_amc_channel(argv[2], &channel) != 0) {
						return (-1);
					}
					if (argc > 3){
						if (is_amc_dev(argv[3], &device) != 0) {
							return (-1);
						}
					}else{
					   device = -1;
				    }
					lprintf(LOG_DEBUG,"PICMG: requesting device %d",device);
					lprintf(LOG_DEBUG,"PICMG: requesting channel %d",channel);

					rc = ipmi_picmg_amc_portstate_get(intf,device,channel,
                                             PICMG_EKEY_MODE_QUERY );
				}
				else {
					lprintf(LOG_NOTICE, "<chn> <device>|getall|getgranted|getdenied");
				}
			}
			else if (!strcmp(argv[1], "set")) {
				if (argc > 7) {
					int32_t device = -1;
					int32_t port = 0;
					uint8_t channel = 0;
					uint8_t enable = 0;
					uint8_t group = 0;
					uint8_t type = 0;
					uint8_t typeext = 0;
					if (is_amc_channel(argv[2], &channel) != 0
							|| is_amc_port(argv[3], &port) != 0
							|| is_link_type(argv[4], &type) !=0
							|| is_link_type_ext(argv[5], &typeext) != 0
							|| is_link_group(argv[6], &group) != 0
							|| is_enable(argv[7], &enable) != 0) {
						return (-1);
					}
					if(argc > 8){
						if (is_amc_dev(argv[8], &device) != 0) {
							return (-1);
						}
					}

					lprintf(LOG_DEBUG,"PICMG: channel %d",channel);
					lprintf(LOG_DEBUG,"PICMG: portflags %d",port);
					lprintf(LOG_DEBUG,"PICMG: type %d",type);
					lprintf(LOG_DEBUG,"PICMG: typeext %d",typeext);
					lprintf(LOG_DEBUG,"PICMG: group %d",group);
					lprintf(LOG_DEBUG,"PICMG: enable %d",enable);
					lprintf(LOG_DEBUG,"PICMG: device %d",device);

					rc = ipmi_picmg_amc_portstate_set(intf, channel, port, type,
                                               typeext, group, enable, device);
				}
				else {
					lprintf(LOG_NOTICE,
							"<chn> <portflags> <type> <ext> <group> <1|0> [<device>]");
					return -1;
				}
			}
		}
		else {
			lprintf(LOG_NOTICE, "<set>|<get>|<getall>|<getgranted>|<getdenied>");
			return -1;
		}
	}
	/* ATCA led commands */
	else if (!strcmp(argv[0], "led")) {
		if (argc > 1) {
			if (!strcmp(argv[1], "prop")) {
				if (argc > 2) {
					rc = ipmi_picmg_get_led_properties(intf, &(argv[2]));
				}
				else {
					lprintf(LOG_NOTICE, "led prop <FRU-ID>");
				}
			}
			else if (!strcmp(argv[1], "cap")) {
				if (argc > 3) {
					rc = ipmi_picmg_get_led_capabilities(intf, &(argv[2]));
				}
				else {
					lprintf(LOG_NOTICE, "led cap <FRU-ID> <LED-ID>");
				}
			}
			else if (!strcmp(argv[1], "get")) {
				if (argc > 3) {
					rc = ipmi_picmg_get_led_state(intf, &(argv[2]));
				}
				else {
					lprintf(LOG_NOTICE, "led get <FRU-ID> <LED-ID>");
				}
			}
			else if (!strcmp(argv[1], "set")) {
				if (argc > 6) {
					rc = ipmi_picmg_set_led_state(intf, &(argv[2]));
				}
				else {
					lprintf(LOG_NOTICE,
							"led set <FRU-ID> <LED-ID> <function> <duration> <color>");
					lprintf(LOG_NOTICE, "   <FRU-ID>");
					lprintf(LOG_NOTICE, "   <LED-ID>    0:         Blue LED");
					lprintf(LOG_NOTICE, "               1:         LED 1");
					lprintf(LOG_NOTICE, "               2:         LED 2");
					lprintf(LOG_NOTICE, "               3:         LED 3");
					lprintf(LOG_NOTICE, "               0x04-0xFE: OEM defined");
					lprintf(LOG_NOTICE,
							"               0xFF:      All LEDs under management control");
					lprintf(LOG_NOTICE, "   <function>  0:       LED OFF override");
					lprintf(LOG_NOTICE,
							"               1 - 250: LED blinking override (off duration)");
					lprintf(LOG_NOTICE, "               251:     LED Lamp Test");
					lprintf(LOG_NOTICE,
							"               252:     LED restore to local control");
					lprintf(LOG_NOTICE, "               255:     LED ON override");
					lprintf(LOG_NOTICE, "   <duration>  0 - 127: LED Lamp Test duration");
					lprintf(LOG_NOTICE, "               0 - 255: LED Lamp ON duration");
					lprintf(LOG_NOTICE, "   <color>     0:   reserved");
					lprintf(LOG_NOTICE, "               1:   BLUE");
					lprintf(LOG_NOTICE, "               2:   RED");
					lprintf(LOG_NOTICE, "               3:   GREEN");
					lprintf(LOG_NOTICE, "               4:   AMBER");
					lprintf(LOG_NOTICE, "               5:   ORANGE");
					lprintf(LOG_NOTICE, "               6:   WHITE");
					lprintf(LOG_NOTICE, "               7:   reserved");
					lprintf(LOG_NOTICE, "               0xE: do not change");
					lprintf(LOG_NOTICE, "               0xF: use default color");
				}
			}
			else {
				lprintf(LOG_NOTICE, "prop | cap | get | set");
			}
		}
	}
	/* power commands */
	else if (!strcmp(argv[0], "power")) {
		if (argc > 1) {
			if (!strcmp(argv[1], "get")) {
				if (argc > 3) {
					rc = ipmi_picmg_get_power_level(intf, &(argv[2]));
				}
				else {
					lprintf(LOG_NOTICE, "power get <FRU-ID> <type>");
					lprintf(LOG_NOTICE, "   <type>   0 : steady state power draw levels");
					lprintf(LOG_NOTICE,
							"            1 : desired steady state draw levels");
					lprintf(LOG_NOTICE, "            2 : early power draw levels");
					lprintf(LOG_NOTICE, "            3 : desired early levels");

					return -1;
				}
			}
			else if (!strcmp(argv[1], "set")) {
				if (argc > 4) {
					rc = ipmi_picmg_set_power_level(intf, &(argv[2]));
				}
				else {
					lprintf(LOG_NOTICE, "power set <FRU-ID> <level> <present-desired>");
					lprintf(LOG_NOTICE, "   <level>  0 :        Power Off");
					lprintf(LOG_NOTICE, "            0x1-0x14 : Power level");
					lprintf(LOG_NOTICE, "            0xFF :     do not change");
					lprintf(LOG_NOTICE,
							"\n   <present-desired> 0: do not change present levels");
					lprintf(LOG_NOTICE,
							"                     1: copy desired to present level");

					return -1;
				}
			}
			else {
				lprintf(LOG_NOTICE, "<set>|<get>");
				return -1;
			}
		}
		else {
			lprintf(LOG_NOTICE, "<set>|<get>");
			return -1;
		}
	}/* clk commands*/
	else if (!strcmp(argv[0], "clk")) {
		if (argc > 1) {
			if (!strcmp(argv[1], "get")) {
				int8_t clk_res = -1;            
				uint8_t clk_id;
				uint8_t max_res = 15;

				if( PicmgCardType == PICMG_CARD_TYPE_AMC ) {
					max_res = 0;
				}

				if(!strcmp(argv[1], "getall")) {
					if( verbose ) { printf("Getting all clock state\n") ;}	
					for(clk_res=0;clk_res<=max_res;clk_res++) {
						for(clk_id=0;clk_id<=15;clk_id++) {
								rc = ipmi_picmg_clk_get(intf,clk_id,clk_res,
								        PICMG_EKEY_MODE_PRINT_ALL);
						}
					}
				}
				else if(!strcmp(argv[1], "getdenied")) {
					if( verbose ) { printf("Getting disabled clocks\n") ;}	
					for(clk_res=0;clk_res<=max_res;clk_res++) {
						for(clk_id=0;clk_id<=15;clk_id++) {
								rc = ipmi_picmg_clk_get(intf,clk_id,clk_res,
								        PICMG_EKEY_MODE_PRINT_DISABLED);
						}
					}
				}
				else if(!strcmp(argv[1], "getgranted")) {
					if( verbose ) { printf("Getting enabled clocks\n") ;}	
					for(clk_res=0;clk_res<=max_res;clk_res++) {
						for(clk_id=0;clk_id<=15;clk_id++) {
								rc = ipmi_picmg_clk_get(intf,clk_id,clk_res,
								        PICMG_EKEY_MODE_PRINT_ENABLED);
						}
					}
				}
				else if (argc > 2) {
					if (is_clk_id(argv[2], &clk_id) != 0) {
						return (-1);
					}
					if (argc > 3) {
						if (is_clk_resid(argv[3], &clk_res) != 0) {
							return (-1);
						}
					}

					rc = ipmi_picmg_clk_get(intf, clk_id, clk_res,
							PICMG_EKEY_MODE_QUERY );
				}
				else {
					lprintf(LOG_NOTICE, "clk get");
					lprintf(LOG_NOTICE,
							"<CLK-ID> [<DEV-ID>] |getall|getgranted|getdenied");
					return -1;
				}
			}
			else if (!strcmp(argv[1], "set")) {
				if (argc > 7) {
					rc = ipmi_picmg_clk_set(intf, argc-1, &(argv[2]));
				}
				else {
					lprintf(LOG_NOTICE,
							"clk set <CLK-ID> <index> <setting> <family> <acc-lvl> <freq> [<DEV-ID>]");

					return -1;
				}
			}
			else {
				lprintf(LOG_NOTICE, "<set>|<get>|<getall>|<getgranted>|<getdenied>");
				return -1;
			}
		}
		else {
			lprintf(LOG_NOTICE, "<set>|<get>|<getall>|<getgranted>|<getdenied>");
			return -1;
		}
	}

	else if(showProperties == 0 ){

		ipmi_picmg_help();
		return -1;
	}

	return rc;
}

uint8_t
ipmi_picmg_ipmb_address(struct ipmi_intf *intf) {
	struct ipmi_rq req;
	struct ipmi_rs *rsp;
	uint8_t msg_data;

	if (!intf->picmg_avail) {
		return 0;
	}
	memset(&req, 0, sizeof(req));
	req.msg.netfn = IPMI_NETFN_PICMG;
	req.msg.cmd = PICMG_GET_ADDRESS_INFO_CMD;
	msg_data    = 0x00;
	req.msg.data = &msg_data;
	req.msg.data_len = 1;
	msg_data = 0;

	rsp = intf->sendrecv(intf, &req);
	if (rsp && !rsp->ccode) {
		return rsp->data[2];
	}
	if (rsp) {
		lprintf(LOG_DEBUG, "Get Address Info failed: %#x %s",
			rsp->ccode, val2str(rsp->ccode, completion_code_vals));
	} else {
		lprintf(LOG_DEBUG, "Get Address Info failed: No Response");
	}
	return 0;
}

uint8_t
picmg_discover(struct ipmi_intf *intf) {
	/* Check if PICMG extension is available to use the function 
	 * GetDeviceLocator to retrieve i2c address PICMG hack to set
	 * right IPMB address, If extension is not supported, should 
	 * not give any problems
	 *  PICMG Extension Version 2.0 (PICMG 3.0 Revision 1.0 ATCA) to
	 *  PICMG Extension Version 2.3 (PICMG 3.0 Revision 3.0 ATCA)
	 *  PICMG Extension Version 4.1 (PICMG 3.0 Revision 3.0 AMC)
	 *  PICMG Extension Version 5.0 (MTCA.0 R1.0)
	 */

	/* First, check if PICMG extension is available and supported */
	struct ipmi_rq req;
	struct ipmi_rs *rsp;
	uint8_t msg_data;
	uint8_t picmg_avail = 0;

	memset(&req, 0, sizeof(req));
	req.msg.netfn = IPMI_NETFN_PICMG;
	req.msg.cmd = PICMG_GET_PICMG_PROPERTIES_CMD;
	msg_data    = 0x00;
	req.msg.data = &msg_data;
	req.msg.data_len = 1;
	msg_data = 0;

	lprintf(LOG_DEBUG, "Running Get PICMG Properties my_addr %#x, transit %#x, target %#x",
		intf->my_addr, intf->transit_addr, intf->target_addr);
	rsp = intf->sendrecv(intf, &req);
	if (!rsp) {
	    lprintf(LOG_DEBUG,"No response from Get PICMG Properties");
	} else if (rsp->ccode) {
	    lprintf(LOG_DEBUG,"Error response %#x from Get PICMG Properties",
		    rsp->ccode);
	} else if (rsp->data_len < 4) {
	    lprintf(LOG_INFO,"Invalid Get PICMG Properties response length %d",
		    rsp->data_len);
	} else if (rsp->data[0] != 0) {
	    lprintf(LOG_INFO,"Invalid Get PICMG Properties group extension %#x",
		    rsp->data[0]);
	} else if ((rsp->data[1] & 0x0F) != PICMG_ATCA_MAJOR_VERSION
		&& (rsp->data[1] & 0x0F) != PICMG_AMC_MAJOR_VERSION
		&& (rsp->data[1] & 0x0F) != PICMG_UTCA_MAJOR_VERSION) {
	    lprintf(LOG_INFO,"Unknown PICMG Extension Version %d.%d",
		    (rsp->data[1] & 0x0F), (rsp->data[1] >> 4));
	} else {
	    picmg_avail = 1;
	    lprintf(LOG_DEBUG, "Discovered PICMG Extension Version %d.%d",
		    (rsp->data[1] & 0x0f), (rsp->data[1] >> 4));
	}

	return picmg_avail;
}
