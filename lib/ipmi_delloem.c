/*
 * Copyright (c) 2008, Dell Inc
 * All rights reserved.
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 * - Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the following disclaimer.
 *
 * - Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution.
 * - Neither the name of Dell Inc nor the names of its contributors
 * may be used to endorse or promote products derived from this software 
 * without specific prior written permission.
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE 
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */
/*
 * Thursday Oct 7 17:30:12 2009
 * <deepaganesh_paulraj@dell.com>
 *
 * This code implements a dell OEM proprietary commands.
 * This Code is edited and Implemented the License feature for Delloem
 * Author Harsha S <Harsha_S1@dell.com>
 */
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <unistd.h>
#include <signal.h>
#include <ctype.h>
#include <limits.h>
#include <time.h>

#include <ipmitool/ipmi.h>
#include <ipmitool/ipmi_intf.h>
#include <ipmitool/helper.h>
#include <ipmitool/log.h>
#include <ipmitool/ipmi_sel.h>
#include <ipmitool/ipmi_delloem.h>
#include <ipmitool/ipmi_fru.h>
#include <ipmitool/ipmi_sdr.h>
#include <ipmitool/ipmi_mc.h>
#include <ipmitool/ipmi_sensor.h>
#include <ipmitool/ipmi_sel.h>
#include <ipmitool/bswap.h>
#include <ipmitool/ipmi_sdr.h>
#include <ipmitool/ipmi_entity.h>
#include <ipmitool/ipmi_fru.h>
#include <ipmitool/ipmi_sensor.h>
#include <ipmitool/ipmi_time.h>

#define DELL_OEM_NETFN	(uint8_t)(0x30)
#define GET_IDRAC_VIRTUAL_MAC	(uint8_t)(0xC9)
/* 11g Support Macros */
#define INVALID	(-1)
#define SHARED	0
#define SHARED_WITH_FAILOVER_LOM2	1
#define DEDICATED	2
#define SHARED_WITH_FAILOVER_ALL_LOMS	3
/* 11g Support Macros */
#define SHARED 				0
#define SHARED_WITH_FAILOVER_LOM2 	1
#define DEDICATED 			2
#define SHARED_WITH_FAILOVER_ALL_LOMS 	3
/* 12g Support Strings for nic selection */
#define	INVAILD_FAILOVER_MODE		-2
#define	INVAILD_FAILOVER_MODE_SETTINGS	-3
#define	INVAILD_SHARED_MODE		-4

#define	INVAILD_FAILOVER_MODE_STRING "ERROR: Cannot set shared with failover lom same as current shared lom."
#define	INVAILD_FAILOVER_MODE_SET "ERROR: Cannot set shared with failover loms when NIC is set to dedicated Mode."
#define	INVAILD_SHARED_MODE_SET_STRING "ERROR: Cannot set shared Mode for Blades."

char AciveLOM_String [6] [10] = {
	"None",
	"LOM1",
	"LOM2",
	"LOM3",
	"LOM4",
	"dedicated"
};
/* 11g Support Strings for nic selection */
char NIC_Selection_Mode_String [4] [50] = {
	"shared",
	"shared with failover lom2",
	"dedicated",
	"shared with Failover all loms"
};

char NIC_Selection_Mode_String_12g[] [50] = {
	"dedicated",
	"shared with lom1",
	"shared with lom2",
	"shared with lom3",
	"shared with lom4",
	"shared with failover lom1",
	"shared with failover lom2",
	"shared with failover lom3",
	"shared with failover lom4",
	"shared with failover all loms"
};

const struct vFlashstr vFlash_completion_code_vals[] = {
	{0x00, "SUCCESS"},
	{0x01, "NO_SD_CARD"},
	{0x63, "UNKNOWN_ERROR"},
	{0x00, NULL}
};

static int current_arg =0;
uint8_t iDRAC_FLAG=0;

/*
 * new flags for
 * 11G || 12G || 13G  -> _ALL
 * 12G || 13G -> _12_13
 *
 */
uint8_t iDRAC_FLAG_ALL=0;
uint8_t iDRAC_FLAG_12_13=0;

LCD_MODE lcd_mode;
static uint8_t LcdSupported=0;
static uint8_t SetLEDSupported=0;

volatile uint8_t IMC_Type = IMC_IDRAC_10G;

POWER_HEADROOM powerheadroom;

uint8_t PowercapSetable_flag=0;
uint8_t PowercapstatusFlag=0;

static void usage(void);
/* LCD Function prototypes */
static int ipmi_delloem_lcd_main(struct ipmi_intf *intf, int argc,
		char **argv);
int ipmi_lcd_get_platform_model_name(struct ipmi_intf *intf, char *lcdstring,
		uint8_t max_length, uint8_t field_type);
static int ipmi_idracvalidator_command(struct ipmi_intf *intf);
static int ipmi_lcd_get_configure_command_wh(struct ipmi_intf *intf);
static int ipmi_lcd_get_configure_command(struct ipmi_intf *intf,
		uint8_t *command);
static int ipmi_lcd_set_configure_command(struct ipmi_intf *intf, int command);
static int ipmi_lcd_set_configure_command_wh(struct ipmi_intf *intf, uint32_t  mode,
		uint16_t lcdquallifier,uint8_t errordisp);
static int ipmi_lcd_get_single_line_text(struct ipmi_intf *intf,
		char *lcdstring, uint8_t max_length);
static int ipmi_lcd_get_info_wh(struct ipmi_intf *intf);
static int ipmi_lcd_get_info(struct ipmi_intf *intf);
static int ipmi_lcd_get_status_val(struct ipmi_intf *intf,
		LCD_STATUS *lcdstatus);
static int IsLCDSupported();
static void CheckLCDSupport(struct ipmi_intf *intf);
static void ipmi_lcd_status_print(LCD_STATUS lcdstatus);
static int ipmi_lcd_get_status(struct ipmi_intf *intf);
static int ipmi_lcd_set_kvm(struct ipmi_intf *intf, char status);
static int ipmi_lcd_set_lock(struct ipmi_intf *intf,  char lock);
static int ipmi_lcd_set_single_line_text(struct ipmi_intf *intf, char *text);
static int ipmi_lcd_set_text(struct ipmi_intf *intf, char *text);
static int ipmi_lcd_configure_wh(struct ipmi_intf *intf, uint32_t mode,
		uint16_t lcdquallifier, uint8_t errordisp, char *text);
static int ipmi_lcd_configure(struct ipmi_intf *intf, int command, char *text);
static void ipmi_lcd_usage(void);
/* MAC Function prototypes */
static int ipmi_delloem_mac_main(struct ipmi_intf *intf, int argc, char **argv);
static void InitEmbeddedNICMacAddressValues();
static int ipmi_macinfo_drac_idrac_virtual_mac(struct ipmi_intf *intf,
		uint8_t NicNum);
static int ipmi_macinfo_drac_idrac_mac(struct ipmi_intf *intf,uint8_t NicNum);
static int ipmi_macinfo_10g(struct ipmi_intf *intf, uint8_t NicNum);
static int ipmi_macinfo_11g(struct ipmi_intf *intf, uint8_t NicNum);
static int ipmi_macinfo(struct ipmi_intf *intf, uint8_t NicNum);
static void ipmi_mac_usage(void);
/* LAN Function prototypes */
static int ipmi_delloem_lan_main(struct ipmi_intf *intf, int argc, char **argv);
static int IsLANSupported();
static int get_nic_selection_mode(int current_arg, char **argv);
static int ipmi_lan_set_nic_selection(struct ipmi_intf *intf,
		uint8_t nic_selection);
static int ipmi_lan_get_nic_selection(struct ipmi_intf *intf);
static int ipmi_lan_get_active_nic(struct ipmi_intf *intf);
static void ipmi_lan_usage(void);
static int ipmi_lan_set_nic_selection_12g(struct ipmi_intf *intf,
		uint8_t *nic_selection);
/* Power monitor Function prototypes */
static int ipmi_delloem_powermonitor_main(struct ipmi_intf *intf, int argc,
		char **argv);
static int ipmi_get_sensor_reading(struct ipmi_intf *intf,
		unsigned char sensorNumber, SensorReadingType *pSensorReadingData);
static int ipmi_get_power_capstatus_command(struct ipmi_intf *intf);
static int ipmi_set_power_capstatus_command(struct ipmi_intf *intf,
		uint8_t val);
static int ipmi_powermgmt(struct ipmi_intf *intf);
static int ipmi_powermgmt_clear(struct ipmi_intf *intf, uint8_t clearValue);
static uint64_t watt_to_btuphr_conversion(uint32_t powerinwatt);
static uint32_t btuphr_to_watt_conversion(uint64_t powerinbtuphr);
static int ipmi_get_power_headroom_command(struct ipmi_intf *intf, uint8_t unit);
static int ipmi_get_power_consumption_data(struct ipmi_intf *intf, uint8_t unit);
static int ipmi_get_instan_power_consmpt_data(struct ipmi_intf *intf,
		IPMI_INST_POWER_CONSUMPTION_DATA *instpowerconsumptiondata);
static void ipmi_print_get_instan_power_Amps_data(
		IPMI_INST_POWER_CONSUMPTION_DATA instpowerconsumptiondata);
static int ipmi_print_get_power_consmpt_data(struct ipmi_intf *intf,
		uint8_t  unit);
static int ipmi_get_avgpower_consmpt_history(struct ipmi_intf *intf,
		IPMI_AVGPOWER_CONSUMP_HISTORY *pavgpower);
static int ipmi_get_peakpower_consmpt_history(struct ipmi_intf *intf,
		IPMI_POWER_CONSUMP_HISTORY *pstPeakpower);
static int ipmi_get_minpower_consmpt_history(struct ipmi_intf *intf,
		IPMI_POWER_CONSUMP_HISTORY *pstMinpower);
static int ipmi_print_power_consmpt_history(struct ipmi_intf *intf, int unit);
static int ipmi_get_power_cap(struct ipmi_intf *intf,
		IPMI_POWER_CAP *ipmipowercap);
static int ipmi_print_power_cap(struct ipmi_intf *intf, uint8_t unit);
static int ipmi_set_power_cap(struct ipmi_intf *intf, int unit, int val);
static void ipmi_powermonitor_usage(void);
/* vFlash Function prototypes */
static int ipmi_delloem_vFlash_main(struct ipmi_intf *intf, int argc,
		char **argv);
const char *get_vFlash_compcode_str(uint8_t vflashcompcode,
		const struct vFlashstr *vs);
static int ipmi_get_sd_card_info(struct ipmi_intf *intf);
static int ipmi_delloem_vFlash_process(struct ipmi_intf *intf, int current_arg,
		char **argv);
static void ipmi_vFlash_usage(void);
/* LED Function prototypes */
static int ipmi_getsesmask(int, char **argv);
static void CheckSetLEDSupport(struct ipmi_intf *intf);
static int IsSetLEDSupported(void);
static void ipmi_setled_usage(void);
static int ipmi_delloem_setled_main(struct ipmi_intf *intf, int argc,
		char **argv);
static int ipmi_setled_state(struct ipmi_intf *intf, int bayId, int slotId,
		int state);
static int ipmi_getdrivemap(struct ipmi_intf *intf, int b, int d, int f,
		int *bayId, int *slotId);
static int
get_nic_selection_mode_12g(struct ipmi_intf* intf,int current_arg,
		char ** argv, char *nic_set);

/* Function Name:       ipmi_delloem_main
 *
 * Description:         This function processes the delloem command
 * Input:               intf    - ipmi interface
 *                       argc    - no of arguments
 *                       argv    - argument string array
 * Output:
 *
 * Return:              return code     0 - success
 *                                      -1 - failure
 */
int
ipmi_delloem_main(struct ipmi_intf * intf, int argc, char ** argv)
{
	int rc = 0;
	current_arg = 0;
	if (!argc || !strcmp(argv[0], "help")) {
		usage();
		return 0;
	}
	if (0 ==strcmp(argv[current_arg], "lcd")) {
		rc = ipmi_delloem_lcd_main(intf,argc,argv);
	} else if (!strcmp(argv[current_arg], "mac")) {
		/* mac address*/
		rc = ipmi_delloem_mac_main(intf,argc,argv);
	} else if (!strcmp(argv[current_arg], "lan")) {
		/* lan address*/
		rc = ipmi_delloem_lan_main(intf,argc,argv);
	} else if (!strcmp(argv[current_arg], "setled")) {
		/* SetLED support */
		rc = ipmi_delloem_setled_main(intf,argc,argv);
	} else if (!strcmp(argv[current_arg], "powermonitor")) {
		/*Powermanagement report processing*/
		rc = ipmi_delloem_powermonitor_main(intf,argc,argv);
	} else if (!strcmp(argv[current_arg], "vFlash")) {
		/* vFlash Support */
		rc = ipmi_delloem_vFlash_main(intf,argc,argv);
	} else {
		usage();
		return -1;
	}
	return rc;
}
/*
 * Function Name:     usage
 *
 * Description:       This function prints help message for delloem command
 * Input:
 * Output:
 *
 * Return:
 *
 */
static void
usage(void)
{
	lprintf(LOG_NOTICE,
"");
	lprintf(LOG_NOTICE,
"usage: delloem <command> [option...]");
	lprintf(LOG_NOTICE,
"");
	lprintf(LOG_NOTICE,
"commands:");
	lprintf(LOG_NOTICE,
"    lcd");
	lprintf(LOG_NOTICE,
"    mac");
	lprintf(LOG_NOTICE,
"    lan");
	lprintf(LOG_NOTICE,
"    setled");
	lprintf(LOG_NOTICE,
"    powermonitor");
	lprintf(LOG_NOTICE,
"    vFlash");
	lprintf(LOG_NOTICE,
"");
	lprintf(LOG_NOTICE,
"For help on individual commands type:");
	lprintf(LOG_NOTICE,
"delloem <command> help");
}
/*
 * Function Name:       ipmi_delloem_lcd_main
 *
 * Description:         This function processes the delloem lcd command
 * Input:               intf    - ipmi interface
 *                       argc    - no of arguments
 *                       argv    - argument string array
 * Output:
 *
 * Return:              return code     0 - success
 *                         -1 - failure
 *
 */
static int
ipmi_delloem_lcd_main(struct ipmi_intf * intf, int argc, char ** argv)
{
	int rc = 0;
	current_arg++;
	if (argc < current_arg) {
		usage();
		return -1;
	}
	/* ipmitool delloem lcd info*/
	if (argc == 1 || !strcmp(argv[current_arg], "help")) {
		ipmi_lcd_usage();
		return 0;
	}
	CheckLCDSupport(intf);
	ipmi_idracvalidator_command(intf);
	if (!IsLCDSupported()) {
		lprintf(LOG_ERR, "lcd is not supported on this system.");
		return -1;
	} else if (!strcmp(argv[current_arg], "info")) {
		if (iDRAC_FLAG_ALL) {
			rc = ipmi_lcd_get_info_wh(intf);
		} else {
			rc = ipmi_lcd_get_info(intf);
		}
	} else if (!strcmp(argv[current_arg], "status")) {
		rc = ipmi_lcd_get_status(intf);
	} else if (!strcmp(argv[current_arg], "set")) {
		/* ipmitool delloem lcd set*/
		uint8_t line_number = 0;
		current_arg++;
		if (argc <= current_arg) {
			ipmi_lcd_usage();
			return -1;
		}
		if (!strcmp(argv[current_arg], "line")) {
			current_arg++;
			if (argc <= current_arg) {
				usage();
				return -1;
			}
			if (str2uchar(argv[current_arg], &line_number) != 0) {
				lprintf(LOG_ERR,
						"Argument '%s' is either not a number or out of range.",
						argv[current_arg]);
				return (-1);
			}
			current_arg++;
			if (argc <= current_arg) {
				usage();
				return -1;
			}
		}
		if (!strcmp(argv[current_arg], "mode")
			&& iDRAC_FLAG_ALL)
		{
			current_arg++;
			if (argc <= current_arg) {
				ipmi_lcd_usage();
				return -1;
			}
			if (!argv[current_arg]) {
				ipmi_lcd_usage();
				return -1;
			}
			if (!strcmp(argv[current_arg], "none")) {
				rc = ipmi_lcd_configure_wh(intf, IPMI_DELL_LCD_CONFIG_NONE, 0xFF,
						0XFF, NULL);
			} else if (!strcmp(argv[current_arg], "modelname")) {
				rc = ipmi_lcd_configure_wh(intf, IPMI_DELL_LCD_CONFIG_DEFAULT, 0xFF,
						0XFF, NULL);
			} else if (!strcmp(argv[current_arg], "userdefined")) {
				current_arg++;
				if (argc <= current_arg) {
					ipmi_lcd_usage();
					return -1;
				}
				rc = ipmi_lcd_configure_wh(intf, IPMI_DELL_LCD_CONFIG_USER_DEFINED,
						0xFF, 0XFF, argv[current_arg]);
			} else if (!strcmp(argv[current_arg], "ipv4address")) {
				rc = ipmi_lcd_configure_wh(intf, IPMI_DELL_LCD_iDRAC_IPV4ADRESS,
						0xFF, 0XFF, NULL);
			} else if (!strcmp(argv[current_arg], "macaddress")) {
				rc = ipmi_lcd_configure_wh(intf, IPMI_DELL_LCD_IDRAC_MAC_ADDRESS,
							0xFF, 0XFF, NULL);
			} else if (!strcmp(argv[current_arg], "systemname")) {
				rc = ipmi_lcd_configure_wh(intf, IPMI_DELL_LCD_OS_SYSTEM_NAME, 0xFF,
						0XFF, NULL);
			} else if (!strcmp(argv[current_arg], "servicetag")) {
				rc = ipmi_lcd_configure_wh(intf, IPMI_DELL_LCD_SERVICE_TAG, 0xFF,
						0XFF, NULL);
			} else if (!strcmp(argv[current_arg], "ipv6address")) {
				rc = ipmi_lcd_configure_wh(intf, IPMI_DELL_LCD_iDRAC_IPV6ADRESS,
						0xFF, 0XFF, NULL);
			} else if (!strcmp(argv[current_arg], "ambienttemp")) {
				rc = ipmi_lcd_configure_wh(intf, IPMI_DELL_LCD_AMBEINT_TEMP, 0xFF,
						0XFF, NULL);
			} else if (!strcmp(argv[current_arg], "systemwatt")) {
				rc = ipmi_lcd_configure_wh(intf, IPMI_DELL_LCD_SYSTEM_WATTS, 0xFF,
						0XFF, NULL);
			} else if (!strcmp(argv[current_arg], "assettag")) {
				rc = ipmi_lcd_configure_wh(intf, IPMI_DELL_LCD_ASSET_TAG, 0xFF,
						0XFF, NULL);
			} else if (!strcmp(argv[current_arg], "help")) {
				ipmi_lcd_usage();
			} else {
				lprintf(LOG_ERR, "Invalid DellOEM command: %s",
						argv[current_arg]);
				ipmi_lcd_usage();
			}
		} else if (!strcmp(argv[current_arg], "lcdqualifier")
		           && iDRAC_FLAG_ALL)
		{
			current_arg++;
			if (argc <= current_arg) {
				ipmi_lcd_usage();
				return -1;
			}
			if (!argv[current_arg]) {
				ipmi_lcd_usage();
				return -1;
			}
			if (!strcmp(argv[current_arg], "watt")) {
				rc = ipmi_lcd_configure_wh(intf, 0xFF, 0x00, 0XFF, NULL);
			} else if (!strcmp(argv[current_arg], "btuphr")) {
				rc = ipmi_lcd_configure_wh(intf, 0xFF, 0x01, 0XFF, NULL);
			} else if (!strcmp(argv[current_arg], "celsius")) {
				rc = ipmi_lcd_configure_wh(intf, 0xFF, 0x02, 0xFF, NULL);
			} else if (!strcmp(argv[current_arg], "fahrenheit")) {
				rc = ipmi_lcd_configure_wh(intf, 0xFF, 0x03, 0xFF, NULL);
			} else if (!strcmp(argv[current_arg], "help")) {
				ipmi_lcd_usage();
			} else {
				lprintf(LOG_ERR, "Invalid DellOEM command: %s",
						argv[current_arg]);
				ipmi_lcd_usage();
			}
		} else if (!strcmp(argv[current_arg], "errordisplay")
		           && iDRAC_FLAG_ALL)
		{
			current_arg++;
			if (argc <= current_arg) {
				ipmi_lcd_usage();
				return -1;
			}
			if (!argv[current_arg]) {
				ipmi_lcd_usage();
				return -1;
			}
			if (!strcmp(argv[current_arg], "sel")) {
				rc = ipmi_lcd_configure_wh(intf, 0xFF, 0xFF,
						IPMI_DELL_LCD_ERROR_DISP_SEL, NULL);
			} else if (!strcmp(argv[current_arg], "simple")) {
				rc = ipmi_lcd_configure_wh(intf, 0xFF, 0xFF,
						IPMI_DELL_LCD_ERROR_DISP_VERBOSE, NULL);
			} else if (!strcmp(argv[current_arg], "help")) {
				ipmi_lcd_usage();
			} else {
				lprintf(LOG_ERR, "Invalid DellOEM command: %s",
						argv[current_arg]);
				ipmi_lcd_usage();
			}
		} else if (!strcmp(argv[current_arg], "none")
		           && iDRAC_FLAG==0)
		{
			rc = ipmi_lcd_configure(intf, IPMI_DELL_LCD_CONFIG_NONE, NULL);
		} else if (!strcmp(argv[current_arg], "default")
		           && iDRAC_FLAG==0)
		{
			rc = ipmi_lcd_configure(intf, IPMI_DELL_LCD_CONFIG_DEFAULT, NULL);
		} else if (!strcmp(argv[current_arg], "custom")
		           && iDRAC_FLAG==0)
		{
			current_arg++;
			if (argc <= current_arg) {
				ipmi_lcd_usage();
				return -1;
			}
			rc = ipmi_lcd_configure(intf, IPMI_DELL_LCD_CONFIG_USER_DEFINED,
					argv[current_arg]);
		} else if (!strcmp(argv[current_arg], "vkvm")) {
			current_arg++;
			if (argc <= current_arg) {
				ipmi_lcd_usage();
				return -1;
			}
			if (!strcmp(argv[current_arg], "active")) {
				rc = ipmi_lcd_set_kvm(intf, 1);
			} else if (!strcmp(argv[current_arg], "inactive")) {
				rc = ipmi_lcd_set_kvm(intf, 0);
			} else if (!strcmp(argv[current_arg], "help")) {
				ipmi_lcd_usage();
			} else {
				lprintf(LOG_ERR, "Invalid DellOEM command: %s",
						argv[current_arg]);
				ipmi_lcd_usage();
			}
		} else if (!strcmp(argv[current_arg], "frontpanelaccess")) {
			current_arg++;
			if (argc <= current_arg) {
				ipmi_lcd_usage();
				return -1;
			}
			if (!strcmp(argv[current_arg], "viewandmodify")) {
				rc = ipmi_lcd_set_lock(intf, 0);
			} else if (strcmp(argv[current_arg], "viewonly")==0) {
				rc =  ipmi_lcd_set_lock(intf, 1);
			} else if (strcmp(argv[current_arg], "disabled")==0) {
				rc =  ipmi_lcd_set_lock(intf, 2);
			} else if (!strcmp(argv[current_arg], "help")) {
				ipmi_lcd_usage();
			} else {
				lprintf(LOG_ERR, "Invalid DellOEM command: %s",
						argv[current_arg]);
				ipmi_lcd_usage();
			}
		} else if( (!strcmp(argv[current_arg], "help"))
				&& (iDRAC_FLAG==0)) {
			ipmi_lcd_usage();
		} else {
			lprintf(LOG_ERR, "Invalid DellOEM command: %s",
					argv[current_arg]);
			ipmi_lcd_usage();
			return -1;
		}
	} else {
		lprintf(LOG_ERR, "Invalid DellOEM command: %s",
				argv[current_arg]);
		ipmi_lcd_usage();
		return -1;
	}
	return rc;
}
/* ipmi_lcd_get_platform_model_name - This function retrieves the platform model
 * name, or any other parameter which stores data in the same format
 *
 * @intf:        pointer to interface
 * @lcdstring:   hostname/platform model string(output)
 * @max_length:  length of the platform model string
 * @field_type:  either hostname/platform model
 *
 * returns: 0 => success, other value means error
 */
int
ipmi_lcd_get_platform_model_name(struct ipmi_intf * intf, char* lcdstring,
		uint8_t max_length, uint8_t field_type)
{
	int bytes_copied = 0;
	int ii = 0;
	int lcdstring_len = 0;
	int rc = 0;
	IPMI_DELL_LCD_STRING lcdstringblock;

	for (ii = 0; ii < 4; ii++) {
		int bytes_to_copy;
		rc = ipmi_mc_getsysinfo(intf, field_type, ii, 0, sizeof(lcdstringblock),
				&lcdstringblock);
		if (rc < 0) {
			lprintf(LOG_ERR, "Error getting platform model name");
			break;
		} else if (rc > 0) {
			lprintf(LOG_ERR, "Error getting platform model name: %s",
					val2str(rc, completion_code_vals));
			break;
		}
		/* first block is different - 14 bytes*/
		if (ii == 0) {
			lcdstring_len = lcdstringblock.lcd_string.selector_0_string.length;
			lcdstring_len = MIN(lcdstring_len,max_length);
			bytes_to_copy = MIN(lcdstring_len, IPMI_DELL_LCD_STRING1_SIZE);
			memcpy(lcdstring, lcdstringblock.lcd_string.selector_0_string.data,
					bytes_to_copy);
		} else {
			int string_offset;
			bytes_to_copy = MIN(lcdstring_len - bytes_copied,
					IPMI_DELL_LCD_STRINGN_SIZE);
			if (bytes_to_copy < 1) {
				break;
			}
			string_offset = IPMI_DELL_LCD_STRING1_SIZE + IPMI_DELL_LCD_STRINGN_SIZE
				* (ii-1);
			memcpy(lcdstring + string_offset,
					lcdstringblock.lcd_string.selector_n_data, bytes_to_copy);
		}
		bytes_copied += bytes_to_copy;
		if (bytes_copied >= lcdstring_len) {
			break;
		}
	}
	return rc;
}
/*
 * Function Name:    ipmi_idracvalidator_command
 *
 * Description:      This function returns the iDRAC6 type
 * Input:            intf            - ipmi interface
 * Output:
 *
 * Return:           iDRAC6 type     1 - whoville
 *                                   0 - others
 */
static int
ipmi_idracvalidator_command(struct ipmi_intf * intf)
{
	int rc;
	uint8_t data[11];
	rc = ipmi_mc_getsysinfo(intf, IPMI_DELL_IDRAC_VALIDATOR, 2, 0, sizeof(data),
			data);
	if (rc < 0) {
		/*lprintf(LOG_ERR, " Error getting IMC type"); */
		return -1;
	} else if (rc > 0) {
		/*lprintf(LOG_ERR, " Error getting IMC type: %s",
		val2str(rsp->ccode, completion_code_vals));  */
		return -1;
	}
	/*
	 * Set the new flags to 0
	 */
	iDRAC_FLAG_ALL = 0;
	iDRAC_FLAG_12_13 = 0;
	/* Support the 11G Monolithic, modular, Maisy and Coaster */
	if ((IMC_IDRAC_11G_MONOLITHIC == data[10])
			|| (IMC_IDRAC_11G_MODULAR == data[10])
			|| (IMC_MASER_LITE_BMC == data[10])
			|| (IMC_MASER_LITE_NU == data[10])) {
		iDRAC_FLAG=IDRAC_11G;
		iDRAC_FLAG_ALL = 1;
	} else if((IMC_IDRAC_12G_MONOLITHIC == data[10])
			|| (IMC_IDRAC_12G_MODULAR == data[10])) {
		iDRAC_FLAG = IDRAC_12G;
		iDRAC_FLAG_ALL = 1;
		iDRAC_FLAG_12_13 = 1;
	} else if ((IMC_IDRAC_13G_MONOLITHIC == data[10])
			|| (IMC_IDRAC_13G_MODULAR == data[10])
			|| (IMC_IDRAC_13G_DCS == data[10])) {
		iDRAC_FLAG=IDRAC_13G;
		iDRAC_FLAG_ALL = 1;
		iDRAC_FLAG_12_13 = 1;
	} else {
		iDRAC_FLAG = 0;
		iDRAC_FLAG_ALL = 0;
		iDRAC_FLAG_12_13 = 0;
	}
	IMC_Type = data[10];
	return 0;
}
/*
 * Function Name:    ipmi_lcd_get_configure_command_wh
 *
 * Description:      This function returns current lcd configuration for Dell OEM LCD command
 * Input:            intf            - ipmi interface
 * Global:           lcd_mode - lcd mode setting
 * Output:
 *
 * Return:           returns the current lcd configuration
 *                   0 = User defined
 *                   1 = Default
 *                   2 = None
 */
static int
ipmi_lcd_get_configure_command_wh(struct ipmi_intf * intf)
{
	int rc;
	rc = ipmi_mc_getsysinfo(intf, IPMI_DELL_LCD_CONFIG_SELECTOR, 0, 0,
			sizeof(lcd_mode), &lcd_mode);
	if (rc < 0) {
		lprintf(LOG_ERR, "Error getting LCD configuration");
		return -1;
	} else if ((rc == 0xc1) || (rc == 0xcb)){
		lprintf(LOG_ERR, "Error getting LCD configuration: "
				"Command not supported on this system.");
	} else if (rc > 0) {
		lprintf(LOG_ERR, "Error getting LCD configuration: %s",
				val2str(rc, completion_code_vals));
		return -1;
	}
	return 0;
}
/*
 * Function Name:    ipmi_lcd_get_configure_command
 *
 * Description:   This function returns current lcd configuration for Dell OEM
 * LCD command
 * Input:         intf            - ipmi interface
 * Output:        command         - user defined / default / none / ipv4 / mac address /
 *                 system name / service tag / ipv6 / temp / system watt / asset tag
 *
 * Return:
 */
static int
ipmi_lcd_get_configure_command(struct ipmi_intf * intf, uint8_t *command)
{
	uint8_t data[4];
	int rc;
	rc = ipmi_mc_getsysinfo(intf, IPMI_DELL_LCD_CONFIG_SELECTOR, 0, 0,
			sizeof(data), data);
	if (rc < 0) {
		lprintf(LOG_ERR, "Error getting LCD configuration");
		return -1;
	} else if ((rc == 0xc1)||(rc == 0xcb)) {
		lprintf(LOG_ERR, "Error getting LCD configuration: "
				"Command not supported on this system.");
		return -1;
	} else if (rc > 0) {
		lprintf(LOG_ERR, "Error getting LCD configuration: %s",
				val2str(rc, completion_code_vals));
		return -1;
	}
	/* rsp->data[0] is the rev */
	*command = data[1];
	return 0;
}
/*
 * Function Name:    ipmi_lcd_set_configure_command
 *
 * Description:      This function updates current lcd configuration
 * Input:            intf            - ipmi interface
 *                   command         - user defined / default / none / ipv4 / mac address /
 *                        system name / service tag / ipv6 / temp / system watt / asset tag
 * Output:
 * Return:
 */
static int
ipmi_lcd_set_configure_command(struct ipmi_intf * intf, int command)
{
	#define LSCC_DATA_LEN 2
	uint8_t data[2];
	int rc;
	data[0] = IPMI_DELL_LCD_CONFIG_SELECTOR;
	data[1] = command;                      /* command - custom, default, none */
	rc = ipmi_mc_setsysinfo(intf, 2, data);
	if (rc < 0) {
		lprintf(LOG_ERR, "Error setting LCD configuration");
		return -1;
	} else if ((rc == 0xc1) || (rc == 0xcb)) {
		lprintf(LOG_ERR, "Error setting LCD configuration: "
				"Command not supported on this system.");
	} else if (rc > 0) {
		lprintf(LOG_ERR, "Error setting LCD configuration: %s",
				val2str(rc, completion_code_vals));
		return -1;
	}
	return 0;
}
/*
 * Function Name:    ipmi_lcd_set_configure_command
 *
 * Description:      This function updates current lcd configuration
 * Input:            intf            - ipmi interface
 *                   mode            - user defined / default / none
 *                   lcdquallifier   - lcd quallifier id
 *                   errordisp       - error number
 * Output:
 * Return:
 */
static int
ipmi_lcd_set_configure_command_wh(struct ipmi_intf * intf, uint32_t  mode,
		uint16_t lcdquallifier, uint8_t errordisp)
{
	#define LSCC_DATA_LEN 2
	uint8_t data[13];
	int rc;
	ipmi_lcd_get_configure_command_wh(intf);
	data[0] = IPMI_DELL_LCD_CONFIG_SELECTOR;
	if (mode != 0xFF) {
		data[1] = mode & 0xFF; /* command - custom, default, none*/
		data[2] = (mode & 0xFF00) >> 8;
		data[3] = (mode & 0xFF0000) >> 16;
		data[4] = (mode & 0xFF000000) >> 24;
	} else {
		data[1] = (lcd_mode.lcdmode) & 0xFF; /* command - custom, default, none*/
		data[2] = ((lcd_mode.lcdmode) & 0xFF00) >> 8;
		data[3] = ((lcd_mode.lcdmode) & 0xFF0000) >> 16;
		data[4] = ((lcd_mode.lcdmode) & 0xFF000000) >> 24;
	}
	if (lcdquallifier != 0xFF) {
		if(lcdquallifier == 0x01) {
			data[5] = (lcd_mode.lcdquallifier) | 0x01; /* command - custom, default, none*/
		} else  if (lcdquallifier == 0x00) {
			data[5] = (lcd_mode.lcdquallifier) & 0xFE; /* command - custom, default, none*/
		} else if (lcdquallifier == 0x03) {
			data[5] = (lcd_mode.lcdquallifier) | 0x02; /* command - custom, default, none*/
		} else if (lcdquallifier == 0x02) {
			data[5] = (lcd_mode.lcdquallifier) & 0xFD;
		}
	} else {
		data[5] = lcd_mode.lcdquallifier;
	}
	if (errordisp != 0xFF) {
		data[11] = errordisp;
	} else {
		data[11] = lcd_mode.error_display;
	}
	rc = ipmi_mc_setsysinfo(intf, 13, data);
	if (rc < 0) {
		lprintf(LOG_ERR, "Error setting LCD configuration");
		return -1;
	} else if ((rc == 0xc1) || (rc == 0xcb)) {
		lprintf(LOG_ERR, "Error setting LCD configuration: "
				"Command not supported on this system.");
	} else if (rc > 0) {
		lprintf(LOG_ERR, "Error setting LCD configuration: %s",
				val2str(rc, completion_code_vals));
		return -1;
	}
	return 0;
}
/*
 * Function Name:    ipmi_lcd_get_single_line_text
 *
 * Description:    This function updates current lcd configuration
 * Input:          intf            - ipmi interface
 *                 lcdstring       - new string to be updated
 *                 max_length      - length of the string
 * Output:
 * Return:
 */
static int
ipmi_lcd_get_single_line_text(struct ipmi_intf * intf, char* lcdstring,
		uint8_t max_length)
{
	IPMI_DELL_LCD_STRING lcdstringblock;
	int lcdstring_len = 0;
	int bytes_copied = 0;
	int ii, rc;
	for (ii = 0; ii < 4; ii++) {
		int bytes_to_copy;
		rc = ipmi_mc_getsysinfo(intf, IPMI_DELL_LCD_STRING_SELECTOR, ii, 0,
				sizeof(lcdstringblock), &lcdstringblock);
		if (rc < 0) {
			lprintf(LOG_ERR, "Error getting text data");
			return -1;
		} else if (rc > 0) {
			lprintf(LOG_ERR, "Error getting text data: %s",
					val2str(rc, completion_code_vals));
			return -1;
		}
		/* first block is different - 14 bytes*/
		if (0 == ii) {
			lcdstring_len = lcdstringblock.lcd_string.selector_0_string.length;
			if (lcdstring_len < 1 || lcdstring_len > max_length) {
				break;
			}
			bytes_to_copy = MIN(lcdstring_len, IPMI_DELL_LCD_STRING1_SIZE);
			memcpy(lcdstring, lcdstringblock.lcd_string.selector_0_string.data,
					bytes_to_copy);
		} else {
			int string_offset;
			bytes_to_copy = MIN(lcdstring_len - bytes_copied,
					IPMI_DELL_LCD_STRINGN_SIZE);
			if (bytes_to_copy < 1) {
				break;
			}
			string_offset = IPMI_DELL_LCD_STRING1_SIZE + IPMI_DELL_LCD_STRINGN_SIZE
				* (ii-1);
			memcpy(lcdstring+string_offset,
					lcdstringblock.lcd_string.selector_n_data, bytes_to_copy);
		}
		bytes_copied += bytes_to_copy;
		if (bytes_copied >= lcdstring_len) {
			break;
		}
	}
	return 0;
}
/*
 * Function Name:    ipmi_lcd_get_info_wh
 *
 * Description:     This function prints current lcd configuration for whoville platform
 * Input:           intf            - ipmi interface
 * Output:
 * Return:
 */
static int
ipmi_lcd_get_info_wh(struct ipmi_intf * intf)
{
	IPMI_DELL_LCD_CAPS lcd_caps;
	char lcdstring[IPMI_DELL_LCD_STRING_LENGTH_MAX+1] = {0};
	int rc;
	printf("LCD info\n");
	if (ipmi_lcd_get_configure_command_wh(intf) != 0) {
		return -1;
	}
	if (lcd_mode.lcdmode== IPMI_DELL_LCD_CONFIG_DEFAULT) {
		char text[IPMI_DELL_LCD_STRING_LENGTH_MAX+1] = {0};
		if (ipmi_lcd_get_platform_model_name(intf, text,
					IPMI_DELL_LCD_STRING_LENGTH_MAX,
					IPMI_DELL_PLATFORM_MODEL_NAME_SELECTOR) != 0) {
			return (-1);
		}
		printf("    Setting:Model name\n");
		printf("    Line 1:  %s\n", text);
	} else if (lcd_mode.lcdmode == IPMI_DELL_LCD_CONFIG_NONE) {
		printf("    Setting:   none\n");
	} else if (lcd_mode.lcdmode == IPMI_DELL_LCD_CONFIG_USER_DEFINED) {
		printf("    Setting: User defined\n");
		rc = ipmi_mc_getsysinfo(intf, IPMI_DELL_LCD_GET_CAPS_SELECTOR, 0, 0,
				sizeof(lcd_caps), &lcd_caps);
		if (rc < 0) {
			lprintf(LOG_ERR, "Error getting LCD capabilities.");
			return -1;
		} else if ((rc == 0xc1) || (rc == 0xcb)) {
			lprintf(LOG_ERR, "Error getting LCD capabilities: "
					"Command not supported on this system.");
		} else if (rc > 0) {
			lprintf(LOG_ERR, "Error getting LCD capabilities: %s",
					val2str(rc, completion_code_vals));
			return -1;
		}
		if (lcd_caps.number_lines > 0) {
			memset(lcdstring, 0, IPMI_DELL_LCD_STRING_LENGTH_MAX + 1);
			rc = ipmi_lcd_get_single_line_text(intf, lcdstring,
					lcd_caps.max_chars[0]);
			printf("    Text:    %s\n", lcdstring);
		} else {
			printf("    No lines to show\n");
		}
	} else if (lcd_mode.lcdmode == IPMI_DELL_LCD_iDRAC_IPV4ADRESS) {
		printf("    Setting:   IPV4 Address\n");
	} else if (lcd_mode.lcdmode == IPMI_DELL_LCD_IDRAC_MAC_ADDRESS) {
		printf("    Setting:   MAC Address\n");
	} else if (lcd_mode.lcdmode == IPMI_DELL_LCD_OS_SYSTEM_NAME) {
		printf("    Setting:   OS System Name\n");
	} else if (lcd_mode.lcdmode == IPMI_DELL_LCD_SERVICE_TAG) {
		printf("    Setting:   System Tag\n");
	} else if (lcd_mode.lcdmode == IPMI_DELL_LCD_iDRAC_IPV6ADRESS) {
		printf("    Setting:  IPV6 Address\n");
	} else if (lcd_mode.lcdmode == IPMI_DELL_LCD_ASSET_TAG) {
		printf("    Setting:  Asset Tag\n");
	} else if (lcd_mode.lcdmode == IPMI_DELL_LCD_AMBEINT_TEMP) {
		printf("    Setting:  Ambient Temp\n");
		if (lcd_mode.lcdquallifier & 0x02) {
			printf("    Unit:  F\n");
		} else {
			printf("    Unit:  C\n");
		}
	} else if (lcd_mode.lcdmode == IPMI_DELL_LCD_SYSTEM_WATTS) {
		printf("    Setting:  System Watts\n");
		if (lcd_mode.lcdquallifier & 0x01) {
			printf("    Unit:  BTU/hr\n");
		} else {
			printf("    Unit:  Watt\n");
		}
	}
	if (lcd_mode.error_display == IPMI_DELL_LCD_ERROR_DISP_SEL) {
		printf("    Error Display:  SEL\n");
	} else if (lcd_mode.error_display == IPMI_DELL_LCD_ERROR_DISP_VERBOSE) {
		printf("    Error Display:  Simple\n");
	}
	return 0;
}
/*
 * Function Name:    ipmi_lcd_get_info
 *
 * Description:      This function prints current lcd configuration for platform other than whoville
 * Input:            intf            - ipmi interface
 * Output:
 * Return:
 */
static int
ipmi_lcd_get_info(struct ipmi_intf * intf)
{
	IPMI_DELL_LCD_CAPS lcd_caps;
	uint8_t command = 0;
	char lcdstring[IPMI_DELL_LCD_STRING_LENGTH_MAX+1] = {0};
	int rc;

	printf("LCD info\n");

	if (ipmi_lcd_get_configure_command(intf, &command) != 0) {
		return -1;
	}
	if (command == IPMI_DELL_LCD_CONFIG_DEFAULT) {
		memset(lcdstring,0,IPMI_DELL_LCD_STRING_LENGTH_MAX+1);
		if (ipmi_lcd_get_platform_model_name(intf, lcdstring,
					IPMI_DELL_LCD_STRING_LENGTH_MAX,
					IPMI_DELL_PLATFORM_MODEL_NAME_SELECTOR) != 0) {
			return (-1);
		}
		printf("    Setting: default\n");
		printf("    Line 1:  %s\n", lcdstring);
	} else if (command == IPMI_DELL_LCD_CONFIG_NONE) {
		printf("    Setting:   none\n");
	} else if (command == IPMI_DELL_LCD_CONFIG_USER_DEFINED) {
		printf("    Setting: custom\n");
		rc = ipmi_mc_getsysinfo(intf, IPMI_DELL_LCD_GET_CAPS_SELECTOR, 0, 0,
				sizeof(lcd_caps), &lcd_caps);
		if (rc < 0) {
			lprintf(LOG_ERR, "Error getting LCD capabilities.");
			return -1;
		} else if ((rc == 0xc1) || (rc == 0xcb)) {
			lprintf(LOG_ERR, "Error getting LCD capabilities: "
					"Command not supported on this system.");
		} else if (rc > 0) {
			lprintf(LOG_ERR, "Error getting LCD capabilities: %s",
					val2str(rc, completion_code_vals));
			return -1;
		}
		if (lcd_caps.number_lines > 0) {
			memset(lcdstring, 0, IPMI_DELL_LCD_STRING_LENGTH_MAX + 1);
			rc = ipmi_lcd_get_single_line_text(intf, lcdstring,
					lcd_caps.max_chars[0]);
			printf("    Text:    %s\n", lcdstring);
		} else {
			printf("    No lines to show\n");
		}
	}
	return 0;
}
/*
 * Function Name:    ipmi_lcd_get_status_val
 *
 * Description:      This function gets current lcd configuration
 * Input:            intf            - ipmi interface
 * Output:           lcdstatus       - KVM Status & Lock Status
 * Return:
 */
static int
ipmi_lcd_get_status_val(struct ipmi_intf * intf, LCD_STATUS* lcdstatus)
{
	int rc;
	rc = ipmi_mc_getsysinfo(intf, IPMI_DELL_LCD_STATUS_SELECTOR, 0, 0,
			sizeof(*lcdstatus), lcdstatus);
	if (rc < 0) {
		lprintf(LOG_ERR, "Error getting LCD Status");
		return -1;
	} else if ((rc == 0xc1) || (rc == 0xcb)) {
		lprintf(LOG_ERR, "Error getting LCD status: "
				"Command not supported on this system.");
		return -1;
	} else if (rc > 0) {
		lprintf(LOG_ERR, "Error getting LCD Status: %s",
				val2str(rc, completion_code_vals));
		return -1;
	}
	return 0;
}
/*
 * Function Name:    IsLCDSupported
 *
 * Description:   This function returns whether lcd supported or not
 * Input:
 * Output:
 * Return:
 */
static int
IsLCDSupported()
{
	return LcdSupported;
}
/*
 * Function Name:         CheckLCDSupport
 *
 * Description:  This function checks whether lcd supported or not
 * Input:        intf            - ipmi interface
 * Output:
 * Return:
 */
static void
CheckLCDSupport(struct ipmi_intf * intf)
{
	int rc;
	LcdSupported = 0;
	rc = ipmi_mc_getsysinfo(intf, IPMI_DELL_LCD_STATUS_SELECTOR, 0, 0, 0, NULL);
	if (rc == 0) {
		LcdSupported = 1;
	}
}
/*
 * Function Name:     ipmi_lcd_status_print
 *
 * Description:    This function prints current lcd configuration KVM Status & Lock Status
 * Input:          lcdstatus - KVM Status & Lock Status
 * Output:
 * Return:
 */
static void
ipmi_lcd_status_print(LCD_STATUS lcdstatus)
{
	switch (lcdstatus.vKVM_status) {
		case 0x00:
			printf("LCD KVM Status :Inactive\n");
			break;
		case 0x01:
			printf("LCD KVM Status :Active\n");
			break;
		default:
			printf("LCD KVM Status :Invalid Status\n");
			break;
	}
	switch (lcdstatus.lock_status) {
		case 0x00:
			printf("LCD lock Status :View and modify\n");
			break;
		case 0x01:
			printf("LCD lock Status :View only\n");
			break;
		case 0x02:
			printf("LCD lock Status :disabled\n");
			break;
		default:
			printf("LCD lock Status :Invalid\n");
			break;
	}
}
/*
 * Function Name:     ipmi_lcd_get_status
 *
 * Description:      This function gets current lcd KVM active status & lcd access mode
 * Input:            intf            - ipmi interface
 * Output:
 * Return:           -1 on error
 *                   0 if successful
 */
static int
ipmi_lcd_get_status(struct ipmi_intf * intf)
{
	int rc=0;
	LCD_STATUS  lcdstatus;
	rc =ipmi_lcd_get_status_val( intf, &lcdstatus);
	if (rc < 0) {
		return -1;
	}
	ipmi_lcd_status_print(lcdstatus);
	return rc;
}
/*
 * Function Name:     ipmi_lcd_set_kvm
 *
 * Description:       This function sets lcd KVM active status
 * Input:             intf            - ipmi interface
 *                    status  - Inactive / Active
 * Output:
 * Return:            -1 on error
 *                    0 if successful
 */
static int
ipmi_lcd_set_kvm(struct ipmi_intf * intf, char status)
{
	#define LSCC_DATA_LEN 2
	LCD_STATUS lcdstatus;
	int rc=0;
	struct ipmi_rs * rsp = NULL;
	struct ipmi_rq req = {0};
	uint8_t data[5];
	rc = ipmi_lcd_get_status_val(intf,&lcdstatus);
	if (rc < 0) {
		return -1;
	}
	req.msg.netfn = IPMI_NETFN_APP;
	req.msg.lun = 0;
	req.msg.cmd = IPMI_SET_SYS_INFO;
	req.msg.data_len = 5;
	req.msg.data = data;
	data[0] = IPMI_DELL_LCD_STATUS_SELECTOR;
	data[1] = status; /* active- inactive */
	data[2] = lcdstatus.lock_status; /* full-veiw-locked */
	rsp = intf->sendrecv(intf, &req);
	if (!rsp) {
		lprintf(LOG_ERR, "Error setting LCD status");
		rc= -1;
	} else if ((rsp->ccode == 0xc1) || (rsp->ccode == 0xcb)) {
		lprintf(LOG_ERR, "Error getting LCD status: "
				"Command not supported on this system.");
		return -1;
	} else if (rsp->ccode) {
		lprintf(LOG_ERR, "Error setting LCD status: %s",
				val2str(rsp->ccode, completion_code_vals));
		rc= -1;
	}
	return rc;
}
/*
 * Function Name:   ipmi_lcd_set_lock
 *
 * Description:     This function sets lcd access mode
 * Input:           intf            - ipmi interface
 *                  lock    - View and modify / View only / Disabled
 * Output:
 * Return:          -1 on error
 *                  0 if successful
 */
static int
ipmi_lcd_set_lock(struct ipmi_intf * intf,  char lock)
{
	#define LSCC_DATA_LEN 2
	LCD_STATUS lcdstatus;
	int rc =0;
	struct ipmi_rs * rsp = NULL;
	struct ipmi_rq req = {0};
	uint8_t data[5];
	rc = ipmi_lcd_get_status_val(intf,&lcdstatus);
	if (rc < 0) {
		return -1;
	}
	req.msg.netfn = IPMI_NETFN_APP;
	req.msg.lun = 0;
	req.msg.cmd = IPMI_SET_SYS_INFO;
	req.msg.data_len = 5;
	req.msg.data = data;
	data[0] = IPMI_DELL_LCD_STATUS_SELECTOR;
	data[1] = lcdstatus.vKVM_status; /* active- inactive */
	data[2] = lock; /* full- veiw-locked */
	rsp = intf->sendrecv(intf, &req);
	if (!rsp) {
		lprintf(LOG_ERR, "Error setting LCD status");
		rc = -1;
	} else if ((rsp->ccode == 0xc1) || (rsp->ccode == 0xcb)) {
		lprintf(LOG_ERR, "Error getting LCD status: "
				"Command not supported on this system.");
		rc = -1;
	} else if (rsp->ccode) {
		lprintf(LOG_ERR, "Error setting LCD status: %s",
				val2str(rsp->ccode, completion_code_vals));
		rc= -1;
	}
	return rc;
}
/*
 * Function Name:   ipmi_lcd_set_single_line_text
 *
 * Description:    This function sets lcd line text
 * Input:          intf            - ipmi interface
 *                 text    - lcd string
 * Output:
 * Return:         -1 on error
 *                 0 if successful
 */
static int
ipmi_lcd_set_single_line_text(struct ipmi_intf * intf, char * text)
{
	uint8_t data[18];
	int bytes_to_store = strlen(text);
	int bytes_stored = 0;
	int ii;
	int rc = 0;
	if (bytes_to_store > IPMI_DELL_LCD_STRING_LENGTH_MAX) {
		lprintf(LOG_ERR, "Out of range Max limit is 62 characters");
		return (-1);
	} else {
		bytes_to_store = MIN(bytes_to_store, IPMI_DELL_LCD_STRING_LENGTH_MAX);
		for (ii = 0; ii < 4; ii++) {
			/*first block, 2 bytes parms and 14 bytes data*/
			if (0 == ii) {
				int size_of_copy = MIN((bytes_to_store - bytes_stored),
						IPMI_DELL_LCD_STRING1_SIZE);
				if (size_of_copy < 0) {
					/* allow 0 string length*/
					break;
				}
				data[0] = IPMI_DELL_LCD_STRING_SELECTOR;
				data[1] = ii; /* block number to use (0)*/
				data[2] = 0; /*string encoding*/
				data[3] = bytes_to_store; /* total string length*/
				memcpy(data + 4, text+bytes_stored, size_of_copy);
				bytes_stored += size_of_copy;
			} else {
				int size_of_copy = MIN((bytes_to_store - bytes_stored),
						IPMI_DELL_LCD_STRINGN_SIZE);
				if (size_of_copy <= 0) {
					break;
				}
				data[0] = IPMI_DELL_LCD_STRING_SELECTOR;
				data[1] = ii; /* block number to use (1,2,3)*/
				memcpy(data + 2, text+bytes_stored, size_of_copy);
				bytes_stored += size_of_copy;
			}
			rc = ipmi_mc_setsysinfo(intf, 18, data);
			if (rc < 0) {
				lprintf(LOG_ERR, "Error setting text data");
				rc = -1;
			} else if (rc > 0) {
				lprintf(LOG_ERR, "Error setting text data: %s",
						val2str(rc, completion_code_vals));
				rc = -1;
			}
		}
	}
	return rc;
}
/*
 * Function Name:   ipmi_lcd_set_text
 *
 * Description:     This function sets lcd line text
 * Input:           intf            - ipmi interface
 *                  text    - lcd string
 * Output:
 * Return:          -1 on error
 *                  0 if successful
 */
static int
ipmi_lcd_set_text(struct ipmi_intf * intf, char * text)
{
	int rc = 0;
	IPMI_DELL_LCD_CAPS lcd_caps;
	rc = ipmi_mc_getsysinfo(intf, IPMI_DELL_LCD_GET_CAPS_SELECTOR, 0, 0,
			sizeof(lcd_caps), &lcd_caps);
	if (rc < 0) {
		lprintf(LOG_ERR, "Error getting LCD capabilities");
		return -1;
	} else if (rc > 0) {
		lprintf(LOG_ERR, "Error getting LCD capabilities: %s",
				val2str(rc, completion_code_vals));
		return -1;
	}
	if (lcd_caps.number_lines > 0) {
		rc = ipmi_lcd_set_single_line_text(intf, text);
	} else {
		lprintf(LOG_ERR, "LCD does not have any lines that can be set");
		rc = -1;
	}
	return rc;
}
/*
 * Function Name:   ipmi_lcd_configure_wh
 *
 * Description:     This function updates the current lcd configuration
 * Input:           intf            - ipmi interface
 *                  lcdquallifier- lcd quallifier
 *                  errordisp       - error number
 *                  text            - lcd string
 * Output:
 * Return:          -1 on error
 *                  0 if successful
 */
static int
ipmi_lcd_configure_wh(struct ipmi_intf * intf, uint32_t  mode,
		uint16_t lcdquallifier, uint8_t errordisp, char * text)
{
	int rc = 0;
	if (IPMI_DELL_LCD_CONFIG_USER_DEFINED == mode) {
		/* Any error was reported earlier. */
		rc = ipmi_lcd_set_text(intf, text);
	}
	if (rc == 0) {
		rc = ipmi_lcd_set_configure_command_wh(intf, mode ,lcdquallifier,errordisp);
	}
	return rc;
}
/*
 * Function Name:   ipmi_lcd_configure
 *
 * Description:     This function updates the current lcd configuration
 * Input:           intf            - ipmi interface
 *                  command- lcd command
 *                  text            - lcd string
 * Output:
 * Return:          -1 on error
 *                  0 if successful
 */
static int
ipmi_lcd_configure(struct ipmi_intf * intf, int command, char * text)
{
	int rc = 0;
	if (IPMI_DELL_LCD_CONFIG_USER_DEFINED == command) {
		rc = ipmi_lcd_set_text(intf, text);
	}
	if (rc == 0) {
		rc = ipmi_lcd_set_configure_command(intf, command);
	}
	return rc;
}
/*
 * Function Name:   ipmi_lcd_usage
 *
 * Description:   This function prints help message for lcd command
 * Input:
 * Output:
 *
 * Return:
 */
static void
ipmi_lcd_usage(void)
{
	lprintf(LOG_NOTICE,
"");
	lprintf(LOG_NOTICE,
"Generic DELL HW:");
	lprintf(LOG_NOTICE,
"   lcd set {none}|{default}|{custom <text>}");
	lprintf(LOG_NOTICE,
"      Set LCD text displayed during non-fault conditions");
	lprintf(LOG_NOTICE,
"");
	lprintf(LOG_NOTICE,
"iDRAC 11g or iDRAC 12g or  iDRAC 13g :");
	lprintf(LOG_NOTICE,
"   lcd set {mode}|{lcdqualifier}|{errordisplay}");
	lprintf(LOG_NOTICE,
"      Allows you to set the LCD mode and user-defined string.");
	lprintf(LOG_NOTICE,
"");
	lprintf(LOG_NOTICE,
"   lcd set mode {none}|{modelname}|{ipv4address}|{macaddress}|");
	lprintf(LOG_NOTICE,
"   {systemname}|{servicetag}|{ipv6address}|{ambienttemp}");
	lprintf(LOG_NOTICE,
"   {systemwatt }|{assettag}|{userdefined}<text>");
	lprintf(LOG_NOTICE,
"	   Allows you to set the LCD display mode to any of the preceding");
	lprintf(LOG_NOTICE,
"      parameters");
	lprintf(LOG_NOTICE,
"");
	lprintf(LOG_NOTICE,
"   lcd set lcdqualifier {watt}|{btuphr}|{celsius}|{fahrenheit}");
	lprintf(LOG_NOTICE,
"      Allows you to set the unit for the system ambient temperature mode.");
	lprintf(LOG_NOTICE,
"");
	lprintf(LOG_NOTICE,
"   lcd set errordisplay {sel}|{simple}");
	lprintf(LOG_NOTICE,
"      Allows you to set the error display.");
	lprintf(LOG_NOTICE,
"");
	lprintf(LOG_NOTICE,
"   lcd info");
	lprintf(LOG_NOTICE,
"      Show LCD text that is displayed during non-fault conditions");
	lprintf(LOG_NOTICE,
"");
	lprintf(LOG_NOTICE,
"");
	lprintf(LOG_NOTICE,
"   lcd set vkvm{active}|{inactive}");
	lprintf(LOG_NOTICE,
"      Set vKVM active and inactive, message will be displayed on lcd");
	lprintf(LOG_NOTICE,
"      when vKVM is active and vKVM session is in progress");
	lprintf(LOG_NOTICE,
"");
	lprintf(LOG_NOTICE,
"   lcd set frontpanelaccess {viewandmodify}|{viewonly}|{disabled}");
	lprintf(LOG_NOTICE,
"      Set LCD mode to view and modify, view only or disabled ");
	lprintf(LOG_NOTICE,
"");
	lprintf(LOG_NOTICE,
"   lcd status");
	lprintf(LOG_NOTICE,
"      Show LCD Status for vKVM display<active|inactive>");
	lprintf(LOG_NOTICE,
"      and Front Panel access mode {viewandmodify}|{viewonly}|{disabled}");
	lprintf(LOG_NOTICE,
"");
}
/*
 * Function Name:       ipmi_delloem_mac_main
 *
 * Description:         This function processes the delloem mac command
 * Input:               intf    - ipmi interface
 *                       argc    - no of arguments
 *                       argv    - argument string array
 * Output:
 *
 * Return:              return code     0 - success
 *                         -1 - failure
 */
static int
ipmi_delloem_mac_main(struct ipmi_intf * intf, int argc, char ** argv)
{
	int rc = 0;
	int currIdInt = -1;
	current_arg++;
	if (argc > 1 && !strcmp(argv[current_arg], "help")) {
		ipmi_mac_usage();
		return 0;
	}
	ipmi_idracvalidator_command(intf);
	if (argc == 1) {
		rc = ipmi_macinfo(intf, 0xff);
	} else if (!strcmp(argv[current_arg], "list")) {
		rc = ipmi_macinfo(intf, 0xff);
	} else if (!strcmp(argv[current_arg], "get")) {
		current_arg++;
		if (!argv[current_arg]) {
			ipmi_mac_usage();
			return -1;
		}
		if (str2int(argv[current_arg],&currIdInt) != 0) {
			lprintf(LOG_ERR,
					"Invalid NIC number. The NIC number should be between 0-8");
			return -1;
		}
		if ((currIdInt > 8) || (currIdInt < 0)) {
			lprintf(LOG_ERR,
					"Invalid NIC number. The NIC number should be between 0-8");
			return -1;
		}
		rc = ipmi_macinfo(intf, currIdInt);
	} else {
		ipmi_mac_usage();
	}
	return rc;
}

EmbeddedNICMacAddressType EmbeddedNICMacAddress;

EmbeddedNICMacAddressType_10G EmbeddedNICMacAddress_10G;

static void
InitEmbeddedNICMacAddressValues()
{
	uint8_t i;
	uint8_t j;
	for (i = 0; i < MAX_LOM; i++) {
		EmbeddedNICMacAddress.LOMMacAddress[i].BladSlotNumber = 0;
		EmbeddedNICMacAddress.LOMMacAddress[i].MacType = LOM_MACTYPE_RESERVED;
		EmbeddedNICMacAddress.LOMMacAddress[i].EthernetStatus =
			LOM_ETHERNET_RESERVED;
		EmbeddedNICMacAddress.LOMMacAddress[i].NICNumber = 0;
		EmbeddedNICMacAddress.LOMMacAddress[i].Reserved = 0;
		for (j = 0; j < MACADDRESSLENGH; j++) {
			EmbeddedNICMacAddress.LOMMacAddress[i].MacAddressByte[j] = 0;
			EmbeddedNICMacAddress_10G.MacAddress[i].MacAddressByte[j] = 0;
		}
	}
}

uint8_t UseVirtualMacAddress = 0;
static int
ipmi_macinfo_drac_idrac_virtual_mac(struct ipmi_intf* intf,uint8_t NicNum)
{
	struct ipmi_rs * rsp;
	struct ipmi_rq req;
	uint8_t msg_data[30];
	uint8_t VirtualMacAddress [MACADDRESSLENGH];
	uint8_t input_length=0;
	uint8_t j;
	uint8_t i;
	if (NicNum != 0xff && NicNum != IDRAC_NIC_NUMBER) {
		return 0;
	}
	UseVirtualMacAddress = 0;
	input_length = 0;
	msg_data[input_length++] = 1; /*Get*/

	req.msg.netfn = DELL_OEM_NETFN;
	req.msg.lun = 0;
	req.msg.cmd = GET_IDRAC_VIRTUAL_MAC;
	req.msg.data = msg_data;
	req.msg.data_len = input_length;

	rsp = intf->sendrecv(intf, &req);
	if (!rsp || rsp->ccode) {
		return -1;
	}

	if ((IMC_IDRAC_12G_MODULAR == IMC_Type)
			|| (IMC_IDRAC_12G_MONOLITHIC== IMC_Type)
			|| (IMC_IDRAC_13G_MODULAR == IMC_Type)
			|| (IMC_IDRAC_13G_MONOLITHIC== IMC_Type)) {
		/* Get the Chasiss Assigned MAC Address for 12g Only */
		memcpy(VirtualMacAddress, ((rsp->data) + 1), MACADDRESSLENGH);
		for (i = 0; i < MACADDRESSLENGH; i++) {
			if (VirtualMacAddress[i] != 0) {
				UseVirtualMacAddress = 1;
			}
		}
		/* Get the Server Assigned MAC Address for 12g Only */
		if (!UseVirtualMacAddress) {
			memcpy(VirtualMacAddress, ((rsp->data) + 1 + MACADDRESSLENGH),
					MACADDRESSLENGH);
			for (i = 0; i < MACADDRESSLENGH; i++) {
				if (VirtualMacAddress[i] != 0) {
					UseVirtualMacAddress = 1;
				}
			}
		}
	} else {
		memcpy(VirtualMacAddress, ((rsp->data) + VIRTUAL_MAC_OFFSET),
				MACADDRESSLENGH);
		for (i = 0; i < MACADDRESSLENGH; i++) {
			if (VirtualMacAddress[i] != 0) {
				UseVirtualMacAddress = 1;
			}
		}
	}
	if (UseVirtualMacAddress == 0) {
		return -1;
	}
	if (IMC_IDRAC_10G == IMC_Type) {
		printf("\nDRAC MAC Address ");
	} else if ((IMC_IDRAC_11G_MODULAR == IMC_Type)
			|| (IMC_IDRAC_11G_MONOLITHIC== IMC_Type)) {
		printf("\niDRAC6 MAC Address ");
	} else if ((IMC_IDRAC_12G_MODULAR == IMC_Type)
			|| (IMC_IDRAC_12G_MONOLITHIC== IMC_Type)) {
		printf("\niDRAC7 MAC Address ");
	} else if ((IMC_IDRAC_13G_MODULAR == IMC_Type)
			|| (IMC_IDRAC_13G_MONOLITHIC== IMC_Type)) {
			printf ("\niDRAC8 MAC Address ");
	} else if ((IMC_MASER_LITE_BMC== IMC_Type)
			|| (IMC_MASER_LITE_NU== IMC_Type)) {
		printf("\nBMC MAC Address ");
	} else {
		printf("\niDRAC6 MAC Address ");
	}

	printf("%s\n", mac2str(VirtualMacAddress));
	return 0;
}
/*
 * Function Name:    ipmi_macinfo_drac_idrac_mac
 *
 * Description:      This function retrieves the mac address of DRAC or iDRAC
 * Input:            NicNum
 * Output:
 * Return:
 */
static int
ipmi_macinfo_drac_idrac_mac(struct ipmi_intf* intf,uint8_t NicNum)
{
	struct ipmi_rs * rsp;
	struct ipmi_rq req;
	uint8_t msg_data[30];
	uint8_t input_length=0;
	uint8_t iDRAC6MacAddressByte[MACADDRESSLENGH];
	uint8_t j;
	ipmi_macinfo_drac_idrac_virtual_mac(intf,NicNum);
	if ((NicNum != 0xff && NicNum != IDRAC_NIC_NUMBER)
			|| UseVirtualMacAddress != 0) {
		return 0;
	}
	input_length = 0;
	msg_data[input_length++] = LAN_CHANNEL_NUMBER;
	msg_data[input_length++] = MAC_ADDR_PARAM;
	msg_data[input_length++] = 0x00;
	msg_data[input_length++] = 0x00;

	req.msg.netfn = TRANSPORT_NETFN;
	req.msg.lun = 0;
	req.msg.cmd = GET_LAN_PARAM_CMD;
	req.msg.data = msg_data;
	req.msg.data_len = input_length;

	rsp = intf->sendrecv(intf, &req);
	if (!rsp) {
		lprintf(LOG_ERR, "Error in getting MAC Address");
		return -1;
	}
	if (rsp->ccode) {
		lprintf(LOG_ERR, "Error in getting MAC Address (%s)",
				val2str(rsp->ccode, completion_code_vals));
		return -1;
	}
	memcpy(iDRAC6MacAddressByte, ((rsp->data) + PARAM_REV_OFFSET),
			MACADDRESSLENGH);

	if (IMC_IDRAC_10G == IMC_Type) {
		printf("\nDRAC MAC Address ");
	} else if ((IMC_IDRAC_11G_MODULAR == IMC_Type)
			|| (IMC_IDRAC_11G_MONOLITHIC== IMC_Type)) {
		printf("\niDRAC6 MAC Address ");
	} else if ((IMC_IDRAC_12G_MODULAR == IMC_Type)
			|| (IMC_IDRAC_12G_MONOLITHIC== IMC_Type)) {
		printf("\niDRAC7 MAC Address ");
	} else if ((IMC_IDRAC_13G_MODULAR == IMC_Type)
			|| (IMC_IDRAC_13G_MONOLITHIC== IMC_Type)) {
			printf ("\niDRAC8 MAC Address ");
	} else if ((IMC_MASER_LITE_BMC== IMC_Type)
			|| (IMC_MASER_LITE_NU== IMC_Type)) {
		printf("\n\rBMC MAC Address ");
	} else {
		printf("\niDRAC6 MAC Address ");
	}

	printf("%s\n", mac2str(iDRAC6MacAddressByte));
	return 0;
}
/*
 * Function Name:    ipmi_macinfo_10g
 *
 * Description:      This function retrieves the mac address of LOMs
 * Input:            intf      - ipmi interface
 *                   NicNum    - NIC number
 * Output:
 * Return:
 */
static int
ipmi_macinfo_10g(struct ipmi_intf* intf, uint8_t NicNum)
{
	struct ipmi_rs * rsp;
	struct ipmi_rq req;
	uint8_t msg_data[30];
	uint8_t input_length=0;
	uint8_t j;
	uint8_t i;
	uint8_t Total_No_NICs = 0;
	InitEmbeddedNICMacAddressValues();
	memset(msg_data, 0, sizeof(msg_data));
	input_length = 0;
	msg_data[input_length++] = 0x00; /* Get Parameter Command */
	msg_data[input_length++] = EMB_NIC_MAC_ADDRESS_9G_10G; /* OEM Param */
	msg_data[input_length++] = 0x00;
	msg_data[input_length++] = 0x00;
	memset(&req, 0, sizeof(req));
	req.msg.netfn = IPMI_NETFN_APP;
	req.msg.lun = 0;
	req.msg.cmd = IPMI_GET_SYS_INFO;
	req.msg.data = msg_data;
	req.msg.data_len = input_length;
	rsp = intf->sendrecv(intf, &req);
	if (!rsp) {
		lprintf(LOG_ERR, "Error in getting MAC Address");
		return -1;
	}
	if (rsp->ccode) {
		lprintf(LOG_ERR, "Error in getting MAC Address (%s)",
				val2str(rsp->ccode, completion_code_vals));
		return -1;
	}
	Total_No_NICs = (uint8_t)rsp->data[0 + PARAM_REV_OFFSET]; /* Byte 1: Total Number of Embedded NICs */
	if (IDRAC_NIC_NUMBER != NicNum) {
		if (0xff == NicNum) {
			printf("\nSystem LOMs");
		}
		printf("\nNIC Number\tMAC Address\n");
		memcpy(&EmbeddedNICMacAddress_10G,
				((rsp->data) + PARAM_REV_OFFSET+TOTAL_N0_NICS_INDEX),
				Total_No_NICs* MACADDRESSLENGH);
		/*Read the LOM type and Mac Addresses */
		for (i = 0; i < Total_No_NICs; i++) {
			if ((0xff == NicNum) || (i == NicNum)) {
				printf("\n%d",i);
				printf("\t\t%s",
					mac2str(EmbeddedNICMacAddress_10G.MacAddress[i].MacAddressByte));
			}
		}
		printf("\n");
	}
	ipmi_macinfo_drac_idrac_mac(intf,NicNum);
	return 0;
}
/*
 * Function Name:      ipmi_macinfo_11g
 *
 * Description:        This function retrieves the mac address of LOMs
 * Input:              intf - ipmi interface
 * Output:
 * Return:
 */
static int
ipmi_macinfo_11g(struct ipmi_intf* intf, uint8_t NicNum)
{
	struct ipmi_rs * rsp;
	struct ipmi_rq req;
	uint8_t input_length = 0;
	uint8_t i;
	uint8_t j;
	uint8_t len;
	uint8_t loop_count;
	uint8_t maxlen;
	uint8_t msg_data[30];
	uint8_t offset;
	offset = 0;
	len = 8; /*eigher 8 or 16 */
	maxlen = 64;
	loop_count = maxlen / len;
	InitEmbeddedNICMacAddressValues();
	memset(msg_data, 0, sizeof(msg_data));
	input_length = 0;
	msg_data[input_length++] = 0x00; /* Get Parameter Command */
	msg_data[input_length++] = EMB_NIC_MAC_ADDRESS_11G; /* OEM Param */
	msg_data[input_length++] = 0x00;
	msg_data[input_length++] = 0x00;
	msg_data[input_length++] = 0x00;
	msg_data[input_length++] = 0x00;

	memset(&req, 0, sizeof(req));
	req.msg.netfn = IPMI_NETFN_APP;
	req.msg.lun = 0;
	req.msg.cmd = IPMI_GET_SYS_INFO;
	req.msg.data = msg_data;
	req.msg.data_len = input_length;

	rsp = intf->sendrecv(intf, &req);
	if (!rsp) {
		lprintf(LOG_ERR, "Error in getting MAC Address");
		return -1;
	}
	if (rsp->ccode) {
		lprintf(LOG_ERR, "Error in getting MAC Address (%s)",
				val2str(rsp->ccode, completion_code_vals));
		return -1;
	}
	len = 8; /*eigher 8 or 16 */
	maxlen = (uint8_t)rsp->data[0 + PARAM_REV_OFFSET];
	loop_count = maxlen / len;
	if (IDRAC_NIC_NUMBER != NicNum) {
		if (0xff == NicNum) {
			printf("\nSystem LOMs");
		}
		printf("\nNIC Number\tMAC Address\t\tStatus\n");
		/*Read the LOM type and Mac Addresses */
		offset=0;
		for (i = 0; i < loop_count; i++, offset = offset + len) {
			input_length = 4;
			msg_data[input_length++] = offset;
			msg_data[input_length++] = len;

			req.msg.netfn = IPMI_NETFN_APP;
			req.msg.lun = 0;
			req.msg.cmd = IPMI_GET_SYS_INFO;
			req.msg.data = msg_data;
			req.msg.data_len = input_length;

			rsp = intf->sendrecv(intf, &req);
			if (!rsp) {
				lprintf(LOG_ERR, "Error in getting MAC Address");
				return -1;
			}
			if (rsp->ccode) {
				lprintf(LOG_ERR, "Error in getting MAC Address (%s)",
						val2str(rsp->ccode, completion_code_vals));
				return -1;
			}
			memcpy(&(EmbeddedNICMacAddress.LOMMacAddress[i]),
					((rsp->data)+PARAM_REV_OFFSET), len);
			if (LOM_MACTYPE_ETHERNET == EmbeddedNICMacAddress.LOMMacAddress[i].MacType) {
				if ((0xff==NicNum)
						|| (NicNum == EmbeddedNICMacAddress.LOMMacAddress[i].NICNumber)) {
					printf("\n%d",EmbeddedNICMacAddress.LOMMacAddress[i].NICNumber);
					printf("\t\t%s", mac2str(EmbeddedNICMacAddress.LOMMacAddress[i].MacAddressByte));

					if (LOM_ETHERNET_ENABLED
							== EmbeddedNICMacAddress.LOMMacAddress[i].EthernetStatus) {
						printf("\tEnabled");
					} else {
						printf("\tDisabled");
					}
				}
			}
		}
		printf("\n");
	}
	ipmi_macinfo_drac_idrac_mac(intf,NicNum);
	return 0;
}
/*
 * Function Name:      ipmi_macinfo
 *
 * Description:     This function retrieves the mac address of LOMs
 * Input:           intf   - ipmi interface
 * Output:
 * Return:
 */
static int
ipmi_macinfo(struct ipmi_intf* intf, uint8_t NicNum)
{
	if (IMC_IDRAC_10G == IMC_Type) {
		return ipmi_macinfo_10g(intf,NicNum);
	} else if ((IMC_IDRAC_11G_MODULAR == IMC_Type
				|| IMC_IDRAC_11G_MONOLITHIC == IMC_Type)
			|| (IMC_IDRAC_12G_MODULAR == IMC_Type
				|| IMC_IDRAC_12G_MONOLITHIC == IMC_Type)
			|| (IMC_IDRAC_13G_MODULAR == IMC_Type
				|| IMC_IDRAC_13G_MONOLITHIC == IMC_Type)
			|| (IMC_MASER_LITE_NU == IMC_Type || IMC_MASER_LITE_BMC== IMC_Type)) {
		return ipmi_macinfo_11g(intf,NicNum);
	} else {
		lprintf(LOG_ERR, "Error in getting MAC Address : Not supported platform");
		return (-1);
	}
}
/*
 * Function Name:     ipmi_mac_usage
 *
 * Description:   This function prints help message for mac command
 * Input:
 * Output:
 *
 * Return:
 */
static void
ipmi_mac_usage(void)
{
	lprintf(LOG_NOTICE,
"");
	lprintf(LOG_NOTICE,
"   mac list");
	lprintf(LOG_NOTICE,
"      Lists the MAC address of LOMs");
	lprintf(LOG_NOTICE,
"");
	lprintf(LOG_NOTICE,
"   mac get <NIC number>");
	lprintf(LOG_NOTICE,
"      Shows the MAC address of specified LOM. 0-7 System LOM, 8- DRAC/iDRAC.");
	lprintf(LOG_NOTICE,
"");
}
/*
 * Function Name:       ipmi_delloem_lan_main
 *
 * Description:         This function processes the delloem lan command
 * Input:               intf    - ipmi interface
 *                      argc    - no of arguments (unused, left in for calling consistency)
 *                      argv    - argument string array
 * Output:
 *
 * Return:              return code     0 - success
 *                         -1 - failure
 */
static int
ipmi_delloem_lan_main(struct ipmi_intf * intf, int __UNUSED__(argc), char ** argv)
{
	int rc = 0;
	int nic_selection = 0;
	char nic_set[2] = {0};
	current_arg++;
	if (!argv[current_arg] || !strcmp(argv[current_arg], "help")) {
		ipmi_lan_usage();
		return 0;
	}
	ipmi_idracvalidator_command(intf);
	if (!IsLANSupported()) {
		lprintf(LOG_ERR, "lan is not supported on this system.");
		return -1;
	} else if (!strcmp(argv[current_arg], "set")) {
		current_arg++;
		if (!argv[current_arg]) {
			ipmi_lan_usage();
			return -1;
		}
		if (iDRAC_FLAG_12_13)  {
			nic_selection = get_nic_selection_mode_12g(intf, current_arg, argv,
					nic_set);
			if (INVALID == nic_selection) {
				ipmi_lan_usage();
				return -1;
			} else if (INVAILD_FAILOVER_MODE == nic_selection) {
				lprintf(LOG_ERR, INVAILD_FAILOVER_MODE_STRING);
				return (-1);
			} else if (INVAILD_FAILOVER_MODE_SETTINGS == nic_selection) {
				lprintf(LOG_ERR, INVAILD_FAILOVER_MODE_SET);
				return (-1);
			} else if (INVAILD_SHARED_MODE == nic_selection) {
				lprintf(LOG_ERR, INVAILD_SHARED_MODE_SET_STRING);
				return (-1);
			}
			rc = ipmi_lan_set_nic_selection_12g(intf,nic_set);
		} else {
			nic_selection = get_nic_selection_mode(current_arg, argv);
			if (INVALID == nic_selection) {
				ipmi_lan_usage();
				return -1;
			}
			if (IMC_IDRAC_11G_MODULAR == IMC_Type) {
				lprintf(LOG_ERR, INVAILD_SHARED_MODE_SET_STRING);
				return (-1);
			}
			rc = ipmi_lan_set_nic_selection(intf,nic_selection);
		}
		return 0;
	} else if (!strcmp(argv[current_arg], "get")) {
		current_arg++;
		if (!argv[current_arg]) {
			rc = ipmi_lan_get_nic_selection(intf);
			return rc;
		} else if (!strcmp(argv[current_arg], "active")) {
			rc = ipmi_lan_get_active_nic(intf);
			return rc;
		} else {
			ipmi_lan_usage();
		}
	} else {
		ipmi_lan_usage();
		return -1;
	}
	return rc;
}

static int
IsLANSupported()
{
	if (IMC_IDRAC_11G_MODULAR == IMC_Type) {
		return 0;
	}
	return 1;
}

static int
get_nic_selection_mode_12g(struct ipmi_intf* intf,int current_arg,
		char ** argv, char *nic_set)
{
	/* First get the current settings. */
	struct ipmi_rs * rsp;
	struct ipmi_rq req;
	int failover = 0;
	uint8_t input_length = 0;
	uint8_t msg_data[30];

	input_length = 0;
	req.msg.netfn = DELL_OEM_NETFN;
	req.msg.lun = 0;
	req.msg.cmd = GET_NIC_SELECTION_12G_CMD;
	req.msg.data = msg_data;
	req.msg.data_len = input_length;
	rsp = intf->sendrecv(intf, &req);
	if (!rsp) {
		lprintf(LOG_ERR, "Error in getting nic selection");
		return -1;
	} else if (rsp->ccode) {
		lprintf(LOG_ERR, "Error in getting nic selection (%s)",
				val2str(rsp->ccode, completion_code_vals));
		return -1;
	}
	nic_set[0] = rsp->data[0];
	nic_set[1] = rsp->data[1];
	if (argv[current_arg]
			&& !strcmp(argv[current_arg], "dedicated")) {
		nic_set[0] = 1;
		nic_set[1] = 0;
		return 0;
	}
	if (argv[current_arg]
			&& !strcmp(argv[current_arg], "shared")) {
		/* placeholder */
	} else {
		return INVALID;
	}

	current_arg++;
	if (argv[current_arg]
			&& !strcmp(argv[current_arg], "with")) {
		/* placeholder */
	} else {
		return INVALID;
	}

	current_arg++;
	if (argv[current_arg]
			&& !strcmp(argv[current_arg], "failover")) {
		failover = 1;
	}
	if (failover) {
		current_arg++;
	}
	if (argv[current_arg]
			&& !strcmp(argv[current_arg], "lom1")) {
		if ((IMC_IDRAC_12G_MODULAR == IMC_Type) || (IMC_IDRAC_13G_MODULAR == IMC_Type)) {
			return INVAILD_SHARED_MODE;
		}
		if (failover) {
			if (nic_set[0] == 2) {
				return INVAILD_FAILOVER_MODE;
			} else if (nic_set[0] == 1) {
				return INVAILD_FAILOVER_MODE_SETTINGS;
			}
			nic_set[1] = 2;
		} else {
			nic_set[0] = 2;
			if (nic_set[1] == 2) {
				nic_set[1] = 0;
			}
		}
		return 0;
	} else if (argv[current_arg]
			&& !strcmp(argv[current_arg], "lom2")) {
		if ((IMC_IDRAC_12G_MODULAR == IMC_Type) || (IMC_IDRAC_13G_MODULAR == IMC_Type)) {
			return INVAILD_SHARED_MODE;
		}
		if (failover) {
			if (nic_set[0] == 3) {
				return INVAILD_FAILOVER_MODE;
			} else if(nic_set[0] == 1) {
				return INVAILD_FAILOVER_MODE_SETTINGS;
			}
			nic_set[1] = 3;
		} else {
			nic_set[0] = 3;
			if (nic_set[1] == 3) {
				nic_set[1] = 0;
			}
		}
		return 0;
	} else if (argv[current_arg]
			&& !strcmp(argv[current_arg], "lom3")) {
		if ((IMC_IDRAC_12G_MODULAR == IMC_Type) || (IMC_IDRAC_13G_MODULAR == IMC_Type)) {
			return INVAILD_SHARED_MODE;
		}
		if (failover) {
			if (nic_set[0] == 4) {
				return INVAILD_FAILOVER_MODE;
			} else if(nic_set[0] == 1) {
				return INVAILD_FAILOVER_MODE_SETTINGS;
			}
			nic_set[1] = 4;
		} else {
			nic_set[0] = 4;
			if (nic_set[1] == 4) {
				nic_set[1] = 0;
			}
		}
		return 0;
	} else if (argv[current_arg]
			&& !strcmp(argv[current_arg], "lom4")) {
		if ((IMC_IDRAC_12G_MODULAR == IMC_Type) || (IMC_IDRAC_13G_MODULAR == IMC_Type)) {
			return INVAILD_SHARED_MODE;
		}
		if (failover) {
			if (nic_set[0] == 5) {
				return INVAILD_FAILOVER_MODE;
			} else if(nic_set[0] == 1) {
				return INVAILD_FAILOVER_MODE_SETTINGS;
			}
			nic_set[1] = 5;
		} else {
			nic_set[0] = 5;
			if (nic_set[1] == 5) {
				nic_set[1] = 0;
			}
		}
		return 0;
	} else if (failover && argv[current_arg]
			&& !strcmp(argv[current_arg], "none")) {
		if ((IMC_IDRAC_12G_MODULAR == IMC_Type) ||  (IMC_IDRAC_13G_MODULAR == IMC_Type) ) {
			return INVAILD_SHARED_MODE;
		}
		if (failover) {
			if (nic_set[0] == 1) {
				return INVAILD_FAILOVER_MODE_SETTINGS;
			}
			nic_set[1] = 0;
		}
		return 0;
	} else if (failover && argv[current_arg]
			&& !strcmp(argv[current_arg], "all")) {
		/* placeholder */
	} else {
		return INVALID;
	}

	current_arg++;
	if (failover && argv[current_arg]
			&& !strcmp(argv[current_arg], "loms")) {
		if ((IMC_IDRAC_12G_MODULAR == IMC_Type) ||  (IMC_IDRAC_13G_MODULAR == IMC_Type)) {
			return INVAILD_SHARED_MODE;
		}
		if (nic_set[0] == 1) {
			return INVAILD_FAILOVER_MODE_SETTINGS;
		}
		nic_set[1] = 6;
		return 0;
	}
	return INVALID;
}

static int
get_nic_selection_mode(int current_arg, char ** argv)
{
	if (argv[current_arg]
			&& !strcmp(argv[current_arg], "dedicated")) {
		return DEDICATED;
	}
	if (argv[current_arg]
			&& !strcmp(argv[current_arg], "shared")) {
		if (!argv[current_arg+1]) {
			return SHARED;
		}
	}

	current_arg++;
	if (argv[current_arg]
			&& !strcmp(argv[current_arg], "with")) {
		/* place holder */
	} else {
		return INVALID;
	}

	current_arg++;
	if (argv[current_arg]
			&& !strcmp(argv[current_arg], "failover")) {
		/* place holder */
	} else {
		return INVALID;
	}

	current_arg++;
	if (argv[current_arg]
			&& !strcmp(argv[current_arg], "lom2")) {
		return SHARED_WITH_FAILOVER_LOM2;
	} else if (argv[current_arg]
			&& !strcmp(argv[current_arg], "all")) {
		/* place holder */
	} else {
		return INVALID;
	}

	current_arg++;
	if (argv[current_arg]
			&& !strcmp(argv[current_arg], "loms")) {
		return SHARED_WITH_FAILOVER_ALL_LOMS;
	}
	return INVALID;
}

static int
ipmi_lan_set_nic_selection_12g(struct ipmi_intf * intf, uint8_t * nic_selection)
{
	struct ipmi_rs * rsp;
	struct ipmi_rq req;
	uint8_t input_length = 0;
	uint8_t msg_data[30];

	input_length = 0;
	msg_data[input_length++] = nic_selection[0];
	msg_data[input_length++] = nic_selection[1];
	req.msg.netfn = DELL_OEM_NETFN;
	req.msg.lun = 0;
	req.msg.cmd = SET_NIC_SELECTION_12G_CMD;
	req.msg.data = msg_data;
	req.msg.data_len = input_length;
	rsp = intf->sendrecv(intf, &req);
	if (!rsp) {
		lprintf(LOG_ERR, "Error in setting nic selection");
		return -1;
	} else if( (nic_selection[0] == 1)
			&& (( iDRAC_FLAG_12_13 )
			&& (rsp->ccode == LICENSE_NOT_SUPPORTED))) {
		/* Check license only for setting the dedicated nic. */
		lprintf(LOG_ERR,
				"FM001 : A required license is missing or expired");
		return -1;
	} else if (rsp->ccode) {
		lprintf(LOG_ERR, "Error in setting nic selection (%s)",
				val2str(rsp->ccode, completion_code_vals));
		return -1;
	}
	printf("configured successfully");
	return 0;
}

static int
ipmi_lan_set_nic_selection(struct ipmi_intf * intf, uint8_t nic_selection)
{
	struct ipmi_rs * rsp;
	struct ipmi_rq req;
	uint8_t input_length = 0;
	uint8_t msg_data[30];

	input_length = 0;
	msg_data[input_length++] = nic_selection;
	req.msg.netfn = DELL_OEM_NETFN;
	req.msg.lun = 0;
	req.msg.cmd = SET_NIC_SELECTION_CMD;
	req.msg.data = msg_data;
	req.msg.data_len = input_length;
	rsp = intf->sendrecv(intf, &req);
	if (!rsp) {
		lprintf(LOG_ERR, "Error in setting nic selection");
		return -1;
	} else if (rsp->ccode) {
		lprintf(LOG_ERR, "Error in setting nic selection (%s)",
				val2str(rsp->ccode, completion_code_vals));
		return -1;
	}
	printf("configured successfully");
	return 0;
}

static int
ipmi_lan_get_nic_selection(struct ipmi_intf * intf)
{
	struct ipmi_rs * rsp;
	struct ipmi_rq req;
	uint8_t input_length=0;
	uint8_t msg_data[30];
	uint8_t nic_selection=-1;
	uint8_t nic_selection_failover = 0;

	input_length = 0;
	req.msg.netfn = DELL_OEM_NETFN;
	req.msg.lun = 0;
	if( iDRAC_FLAG_12_13 ) {
		req.msg.cmd = GET_NIC_SELECTION_12G_CMD;
	} else {
		req.msg.cmd = GET_NIC_SELECTION_CMD;
	}
	req.msg.data = msg_data;
	req.msg.data_len = input_length;
	rsp = intf->sendrecv(intf, &req);
	if (!rsp) {
		lprintf(LOG_ERR, "Error in getting nic selection");
		return -1;
	} else if (rsp->ccode) {
		lprintf(LOG_ERR, "Error in getting nic selection (%s)",
				val2str(rsp->ccode, completion_code_vals));
		return -1;
	}
	nic_selection = rsp->data[0];
	if( iDRAC_FLAG_12_13 ) {
		nic_selection_failover = rsp->data[1];
		if ((nic_selection < 6) && (nic_selection > 0)
				&& (nic_selection_failover < 7)) {
			if(nic_selection == 1) {
				printf("%s\n",NIC_Selection_Mode_String_12g[nic_selection-1]);
			} else if(nic_selection) {
				printf("Shared LOM   :  %s\n",
						NIC_Selection_Mode_String_12g[nic_selection-1]);
				if(nic_selection_failover  == 0) {
					printf("Failover LOM :  None\n");
				} else if(nic_selection_failover >= 2 && nic_selection_failover <= 6) {
					printf("Failover LOM :  %s\n",
							NIC_Selection_Mode_String_12g[nic_selection_failover + 3]);
				}
			}
		} else {
			lprintf(LOG_ERR, "Error Outof bond Value received (%d) (%d)",
					nic_selection,nic_selection_failover);
			return -1;
		}
	} else {
		printf("%s\n",NIC_Selection_Mode_String[nic_selection]);
	}
	return 0;
}

static int
ipmi_lan_get_active_nic(struct ipmi_intf * intf)
{
	struct ipmi_rs * rsp;
	struct ipmi_rq req;
	uint8_t active_nic=0;
	uint8_t current_lom =0;
	uint8_t input_length=0;
	uint8_t msg_data[30];

	input_length = 0;
	msg_data[input_length++] = 0; /* Get Status */
	msg_data[input_length++] = 0; /* Reserved */
	msg_data[input_length++] = 0; /* Reserved */
	req.msg.netfn = DELL_OEM_NETFN;
	req.msg.lun = 0;
	req.msg.cmd = GET_ACTIVE_NIC_CMD;
	req.msg.data = msg_data;
	req.msg.data_len = input_length;
	rsp = intf->sendrecv(intf, &req);
	if (!rsp) {
		lprintf(LOG_ERR, "Error in getting Active LOM Status");
		return -1;
	} else if (rsp->ccode) {
		lprintf(LOG_ERR, "Error in getting Active LOM Status (%s)",
				val2str(rsp->ccode, completion_code_vals));
		return -1;
	}
	current_lom = rsp->data[0];
	input_length = 0;
	msg_data[input_length++] = 1; /* Get Link status */
	msg_data[input_length++] = 0; /* Reserved */
	msg_data[input_length++] = 0; /* Reserved */
	req.msg.netfn = DELL_OEM_NETFN;
	req.msg.lun = 0;
	req.msg.cmd = GET_ACTIVE_NIC_CMD;
	req.msg.data = msg_data;
	req.msg.data_len = input_length;
	rsp = intf->sendrecv(intf, &req);
	if (!rsp) {
		lprintf(LOG_ERR, "Error in getting Active LOM Status");
		return -1;
	} else if (rsp->ccode) {
		lprintf(LOG_ERR, "Error in getting Active LOM Status (%s)",
				val2str(rsp->ccode, completion_code_vals));
		return -1;
	}
	active_nic = rsp->data[1];
	if (current_lom < 6 && active_nic) {
		printf("\n%s\n", AciveLOM_String[current_lom]);
	} else {
		printf("\n%s\n", AciveLOM_String[0]);
	}
	return 0;
}

static void
ipmi_lan_usage(void)
{
	/* TODO:
	 *  - rewrite
	 *  - review
	 *  - make it fit into 80 chars per line
	 *  - this ``shared with Failover None).'' seems like a typo
	 */
	lprintf(LOG_NOTICE,
"");
	lprintf(LOG_NOTICE,
"   lan set <Mode>");
	lprintf(LOG_NOTICE,
"      sets the NIC Selection Mode :");
	lprintf(LOG_NOTICE,
"          on iDRAC12g OR iDRAC13g  :");
	lprintf(LOG_NOTICE,
"              dedicated, shared with lom1, shared with lom2,shared with lom3,shared");
	lprintf(LOG_NOTICE,
"              with lom4,shared with failover lom1,shared with failover lom2,shared");
	lprintf(LOG_NOTICE,
"              with failover lom3,shared with failover lom4,shared with Failover all");
	lprintf(LOG_NOTICE,
"              loms, shared with Failover None).");
	lprintf(LOG_NOTICE,
"          on other systems :");
	lprintf(LOG_NOTICE,
"              dedicated, shared, shared with failover lom2,");
	lprintf(LOG_NOTICE,
"              shared with Failover all loms.");
	lprintf(LOG_NOTICE,
"");
	lprintf(LOG_NOTICE,
"   lan get ");
	lprintf(LOG_NOTICE,
"          on iDRAC12g or iDRAC13g  :");
	lprintf(LOG_NOTICE,
"              returns the current NIC Selection Mode (dedicated, shared with lom1, shared");
	lprintf(LOG_NOTICE,
"              with lom2, shared with lom3, shared with lom4,shared with failover lom1,");
	lprintf(LOG_NOTICE,
"              shared with failover lom2,shared with failover lom3,shared with failover");
	lprintf(LOG_NOTICE,
"              lom4,shared with Failover all loms,shared with Failover None).");
	lprintf(LOG_NOTICE,
"          on other systems :");
	lprintf(LOG_NOTICE,
"              dedicated, shared, shared with failover,");
	lprintf(LOG_NOTICE,
"              lom2, shared with Failover all loms.");
	lprintf(LOG_NOTICE,
"");
	lprintf(LOG_NOTICE,
"   lan get active");
	lprintf(LOG_NOTICE,
"      returns the current active NIC (dedicated, LOM1, LOM2, LOM3, LOM4).");
	lprintf(LOG_NOTICE,
"");
}
/*
 * Function Name:       ipmi_delloem_powermonitor_main
 *
 * Description:         This function processes the delloem powermonitor command
 * Input:               intf    - ipmi interface
 *                       argc    - no of arguments
 *                      argv    - argument string array
 * Output:
 *
 * Return:              return code     0 - success
 *                         -1 - failure
 */
static int
ipmi_delloem_powermonitor_main(struct ipmi_intf * intf, int argc, char ** argv)
{
	int rc = 0;
	current_arg++;
	if (argc > 1 && !strcmp(argv[current_arg], "help")) {
		ipmi_powermonitor_usage();
		return 0;
	}
	ipmi_idracvalidator_command(intf);
	if (argc == 1) {
		rc = ipmi_powermgmt(intf);
	} else if (!strcmp(argv[current_arg], "status")) {
		rc = ipmi_powermgmt(intf);
	} else if (!strcmp(argv[current_arg], "clear")) {
		current_arg++;
		if (!argv[current_arg]) {
			ipmi_powermonitor_usage();
			return -1;
		} else if (!strcmp(argv[current_arg], "peakpower")) {
			rc = ipmi_powermgmt_clear(intf, 1);
		} else if (!strcmp(argv[current_arg], "cumulativepower")) {
			rc = ipmi_powermgmt_clear(intf, 0);
		} else {
			ipmi_powermonitor_usage();
			return -1;
		}
	} else if (!strcmp(argv[current_arg], "powerconsumption")) {
		current_arg++;
		if (!argv[current_arg]) {
			rc = ipmi_print_get_power_consmpt_data(intf,watt);
		} else if (!strcmp(argv[current_arg], "watt")) {
			rc = ipmi_print_get_power_consmpt_data(intf, watt);
		} else if (!strcmp(argv[current_arg], "btuphr")) {
			rc = ipmi_print_get_power_consmpt_data(intf, btuphr);
		} else {
			ipmi_powermonitor_usage();
			return -1;
		}
	} else if (!strcmp(argv[current_arg], "powerconsumptionhistory")) {
		current_arg++;
		if (!argv[current_arg]) {
			rc = ipmi_print_power_consmpt_history(intf,watt);
		} else if (!strcmp(argv[current_arg], "watt")) {
			rc = ipmi_print_power_consmpt_history(intf, watt);
		} else if (!strcmp(argv[current_arg], "btuphr")) {
			rc = ipmi_print_power_consmpt_history(intf, btuphr);
		} else {
			ipmi_powermonitor_usage();
			return -1;
		}
	} else if (!strcmp(argv[current_arg], "getpowerbudget")) {
		current_arg++;
		if (!argv[current_arg]) {
			rc=ipmi_print_power_cap(intf,watt);
		} else if (!strcmp(argv[current_arg], "watt")) {
			rc = ipmi_print_power_cap(intf, watt);
		} else if (!strcmp(argv[current_arg], "btuphr")) {
			rc = ipmi_print_power_cap(intf, btuphr);
		} else {
			ipmi_powermonitor_usage();
			return -1;
		}
	} else if (!strcmp(argv[current_arg], "setpowerbudget")) {
		int val;
		current_arg++;
		if (!argv[current_arg]) {
			ipmi_powermonitor_usage();
			return -1;
		}
		if (strchr(argv[current_arg], '.')) {
			lprintf(LOG_ERR,
					"Cap value in Watts, Btu/hr or percent should be whole number");
			return -1;
		}
		if (str2int(argv[current_arg], &val) != 0) {
			lprintf(LOG_ERR, "Given capacity value '%s' is invalid.",
					argv[current_arg]);
			return (-1);
		}
		current_arg++;
		if (!argv[current_arg]) {
			ipmi_powermonitor_usage();
		} else if (!strcmp(argv[current_arg], "watt")) {
			rc = ipmi_set_power_cap(intf,watt,val);
		} else if (!strcmp(argv[current_arg], "btuphr")) {
			rc = ipmi_set_power_cap(intf, btuphr,val);
		} else if (!strcmp(argv[current_arg], "percent")) {
			rc = ipmi_set_power_cap(intf,percent,val);
		} else {
			ipmi_powermonitor_usage();
			return -1;
		}
	} else if (!strcmp(argv[current_arg], "enablepowercap")) {
		ipmi_set_power_capstatus_command(intf,1);
	} else if (!strcmp(argv[current_arg], "disablepowercap")) {
		ipmi_set_power_capstatus_command(intf,0);
	} else {
		ipmi_powermonitor_usage();
		return -1;
	}
	return rc;
}

/*
 * Function Name:      ipmi_get_sensor_reading
 *
 * Description:        This function retrieves a raw sensor reading
 * Input:              sensorOwner       - sensor owner id
 *                     sensorNumber      - sensor id
 *                     intf              - ipmi interface
 * Output:             sensorReadingData - ipmi response structure
 * Return:             1 on error
 *                     0 if successful
 */
static int
ipmi_get_sensor_reading(struct ipmi_intf *intf, unsigned char sensorNumber,
		SensorReadingType* pSensorReadingData)
{
	struct ipmi_rq req;
	struct ipmi_rs * rsp;
	int rc = 0;
	memset(&req, 0, sizeof(req));
	req.msg.netfn = IPMI_NETFN_SE;
	req.msg.lun = 0;
	req.msg.cmd = GET_SENSOR_READING;
	req.msg.data = &sensorNumber;
	req.msg.data_len = 1;
	if (!pSensorReadingData) {
		return -1;
	}
	memset(pSensorReadingData, 0, sizeof(SensorReadingType));
	rsp = intf->sendrecv(intf, &req);
	if (!rsp) {
		return 1;
	} else if (rsp->ccode) {
		return 1;
	}
	memcpy(pSensorReadingData, rsp->data, sizeof(SensorReadingType));
	/* if there is an error transmitting ipmi command, return error */
	if (rsp->ccode) {
		rc = 1;
	}
	/* if sensor messages are disabled, return error*/
	if ((!(rsp->data[1]& 0xC0)) || ((rsp->data[1] & 0x20))) {
		rc =1;
	}
	return rc;
}
/*
 * Function Name:   ipmi_get_power_capstatus_command
 *
 * Description:     This function gets the power cap status
 * Input:           intf                 - ipmi interface
 * Global:          PowercapSetable_flag - power cap status
 * Output:
 *
 * Return:
 */
static int
ipmi_get_power_capstatus_command(struct ipmi_intf * intf)
{
	struct ipmi_rs * rsp = NULL;
	struct ipmi_rq req = {0};
	uint8_t data[2];
	req.msg.netfn = DELL_OEM_NETFN;
	req.msg.lun = 0;
	req.msg.cmd = IPMI_DELL_POWER_CAP_STATUS;
	req.msg.data_len = 2;
	req.msg.data = data;
	data[0] = 01;
	data[1] = 0xFF;
	rsp = intf->sendrecv(intf, &req);
	if (!rsp) {
		lprintf(LOG_ERR, "Error getting powercap status");
		return -1;
	} else if (( iDRAC_FLAG_12_13 ) && (rsp->ccode == LICENSE_NOT_SUPPORTED)) {
		lprintf(LOG_ERR,
				"FM001 : A required license is missing or expired");
		return -1; /* Return Error as unlicensed */
	} else if (rsp->ccode) {
		lprintf(LOG_ERR, "Error getting powercap statusr: %s",
				val2str(rsp->ccode, completion_code_vals));
		return -1;
	}
	if (rsp->data[0] & 0x02) {
		PowercapSetable_flag=1;
	}
	if (rsp->data[0] & 0x01) {
		PowercapstatusFlag=1;
	}
	return 0;
}
/*
 * Function Name:    ipmi_set_power_capstatus_command
 *
 * Description:      This function sets the power cap status
 * Input:            intf     - ipmi interface
 *                   val      - power cap status
 * Output:
 *
 * Return:
 */
static int
ipmi_set_power_capstatus_command(struct ipmi_intf * intf, uint8_t val)
{
	struct ipmi_rs * rsp = NULL;
	struct ipmi_rq req = {0};
	uint8_t data[2];
	if (ipmi_get_power_capstatus_command(intf) < 0) {
		return -1;
	}
	if (PowercapSetable_flag != 1) {
		lprintf(LOG_ERR, "Can not set powercap on this system");
		return -1;
	}
	req.msg.netfn = DELL_OEM_NETFN;
	req.msg.lun = 0;
	req.msg.cmd = IPMI_DELL_POWER_CAP_STATUS;
	req.msg.data_len = 2;
	req.msg.data = data;
	data[0] = 00;
	data[1] = val;
	rsp = intf->sendrecv(intf, &req);
	if (!rsp) {
		lprintf(LOG_ERR, "Error setting powercap status");
		return -1;
	} else if ((iDRAC_FLAG_12_13) && (rsp->ccode == LICENSE_NOT_SUPPORTED)) {
		lprintf(LOG_ERR,
				"FM001 : A required license is missing or expired");
		return -1; /* return unlicensed Error code */
	} else if (rsp->ccode) {
		lprintf(LOG_ERR, "Error setting powercap statusr: %s",
				val2str(rsp->ccode, completion_code_vals));
		return -1;
	}
	return 0;
}
/*
 * Function Name:    ipmi_powermgmt
 *
 * Description:      This function print the powermonitor details
 * Input:            intf     - ipmi interface
 * Output:
 *
 * Return:
 */
static int
ipmi_powermgmt(struct ipmi_intf * intf)
{
	struct ipmi_rs * rsp;
	struct ipmi_rq req;
	uint8_t msg_data[2];
	uint32_t cumStartTime;
	uint32_t cumReading;
	uint32_t maxPeakStartTime;
	uint32_t ampPeakTime;
	uint32_t wattPeakTime;
	uint32_t bmctime;

	IPMI_POWER_MONITOR * pwrMonitorInfo;

	int ampReading;
	int ampReadingRemainder;
	int remainder;
	int wattReading;

	memset(&req, 0, sizeof(req));
	req.msg.netfn = IPMI_NETFN_STORAGE;
	req.msg.lun = 0;
	req.msg.cmd = IPMI_CMD_GET_SEL_TIME;

	rsp = intf->sendrecv(intf, &req);
	if (!rsp) {
		lprintf(LOG_ERR, "Error getting BMC time info.");
		return -1;
	}
	if (rsp->ccode) {
		lprintf(LOG_ERR,
				"Error getting power management information, return code %x",
				rsp->ccode);
		return -1;
	}
	bmctime = ipmi32toh(rsp->data);

	/* get powermanagement info*/
	req.msg.netfn = DELL_OEM_NETFN;
	req.msg.lun = 0x0;
	req.msg.cmd = GET_PWRMGMT_INFO_CMD;
	req.msg.data = msg_data;
	req.msg.data_len = 2;

	memset(msg_data, 0, 2);
	msg_data[0] = 0x07;
	msg_data[1] = 0x01;

	rsp = intf->sendrecv(intf, &req);
	if (!rsp) {
		lprintf(LOG_ERR, "Error getting power management information.");
		return -1;
	}

	if ((iDRAC_FLAG_12_13) && (rsp->ccode == LICENSE_NOT_SUPPORTED)) {
		lprintf(LOG_ERR,
				"FM001 : A required license is missing or expired");
		return -1;
	} else if ((rsp->ccode == 0xc1)||(rsp->ccode == 0xcb)) {
		lprintf(LOG_ERR, "Error getting power management information: "
				"Command not supported on this system.");
		return -1;
	}else if (rsp->ccode) {
		lprintf(LOG_ERR,
				"Error getting power management information, return code %x",
				rsp->ccode);
		return -1;
	}

	pwrMonitorInfo = (IPMI_POWER_MONITOR*)rsp->data;
	cumStartTime = ipmi32toh(&pwrMonitorInfo->cumStartTime);
	cumReading = ipmi32toh(&pwrMonitorInfo->cumReading);
	maxPeakStartTime = ipmi32toh(&pwrMonitorInfo->maxPeakStartTime);
	ampPeakTime = ipmi32toh(&pwrMonitorInfo->ampPeakTime);
	ampReading = ipmi16toh(&pwrMonitorInfo->ampReading);
	wattPeakTime = ipmi32toh(&pwrMonitorInfo->wattPeakTime);
	wattReading = ipmi16toh(&pwrMonitorInfo->wattReading);

	remainder = (cumReading % 1000);
	cumReading = cumReading / 1000;
	remainder = (remainder + 50) / 100;

	ampReadingRemainder = ampReading % 10;
	ampReading = ampReading / 10;

	printf("Power Tracking Statistics\n");
	printf("Statistic      : Cumulative Energy Consumption\n");
	printf("Start Time     : %s", ipmi_timestamp_numeric(cumStartTime));
	printf("Finish Time    : %s", ipmi_timestamp_numeric(bmctime));
	printf("Reading        : %d.%d kWh\n\n", cumReading, remainder);

	printf("Statistic      : System Peak Power\n");
	printf("Start Time     : %s", ipmi_timestamp_numeric(maxPeakStartTime));
	printf("Peak Time      : %s", ipmi_timestamp_numeric(wattPeakTime));
	printf("Peak Reading   : %d W\n\n", wattReading);

	printf("Statistic      : System Peak Amperage\n");
	printf("Start Time     : %s", ipmi_timestamp_numeric(maxPeakStartTime));
	printf("Peak Time      : %s", ipmi_timestamp_numeric(ampPeakTime));
	printf("Peak Reading   : %d.%d A\n", ampReading, ampReadingRemainder);
	return 0;
}
/*
 * Function Name:    ipmi_powermgmt_clear
 *
 * Description:     This function clears peakpower / cumulativepower value
 * Input:           intf           - ipmi interface
 *                  clearValue     - peakpower / cumulativepower
 * Output:
 *
 * Return:
 */
static int
ipmi_powermgmt_clear(struct ipmi_intf * intf, uint8_t clearValue)
{
	struct ipmi_rs * rsp;
	struct ipmi_rq req;
	uint8_t clearType = 1;
	uint8_t msg_data[3];
	if (clearValue) {
		clearType = 2;
	}
	/* clear powermanagement info*/
	req.msg.netfn = DELL_OEM_NETFN;
	req.msg.lun = 0;
	req.msg.cmd = CLEAR_PWRMGMT_INFO_CMD;
	req.msg.data = msg_data;
	req.msg.data_len = 3;
	memset(msg_data, 0, 3);
	msg_data[0] = 0x07;
	msg_data[1] = 0x01;
	msg_data[2] = clearType;

	rsp = intf->sendrecv(intf, &req);
	if (!rsp) {
		lprintf(LOG_ERR, "Error clearing power values.");
		return -1;
	} else if ((iDRAC_FLAG_12_13)
			&& (rsp->ccode == LICENSE_NOT_SUPPORTED)) {
		lprintf(LOG_ERR,
				"FM001 : A required license is missing or expired");
		return -1;
	} else if (rsp->ccode == 0xc1) {
		lprintf(LOG_ERR,
				"Error clearing power values, command not supported on this system.");
		return -1;
	} else if (rsp->ccode) {
		lprintf(LOG_ERR, "Error clearing power values: %s",
				val2str(rsp->ccode, completion_code_vals));
		return -1;
	}
	return 0;
}
/*
 * Function Name:    watt_to_btuphr_conversion
 *
 * Description:      This function converts the power value in watt to btuphr
 * Input:            powerinwatt     - power in watt
 *
 * Output:           power in btuphr
 *
 * Return:
 */
static uint64_t
watt_to_btuphr_conversion(uint32_t powerinwatt)
{
	uint64_t powerinbtuphr;
	powerinbtuphr=(3.413 * powerinwatt);
	return(powerinbtuphr);
}
/*
 * Function Name:    btuphr_to_watt_conversion
 *
 * Description:      This function converts the power value in  btuphr to watt
 * Input:            powerinbtuphr   - power in btuphr
 *
 * Output:           power in watt
 *
 * Return:
 */
static uint32_t
btuphr_to_watt_conversion(uint64_t powerinbtuphr)
{
	uint32_t powerinwatt;
	/*returning the floor value*/
	powerinwatt= (powerinbtuphr / 3.413);
	return (powerinwatt);
}
/*
 * Function Name:        ipmi_get_power_headroom_command
 *
 * Description:          This function prints the Power consumption information
 * Input:                intf    - ipmi interface
 *                       unit    - watt / btuphr
 * Output:
 *
 * Return:
 */
static int
ipmi_get_power_headroom_command(struct ipmi_intf * intf,uint8_t unit)
{
	struct ipmi_rs * rsp = NULL;
	struct ipmi_rq req = {0};
	uint64_t peakpowerheadroombtuphr;
	uint64_t instantpowerhearoom;

	req.msg.netfn = DELL_OEM_NETFN;
	req.msg.lun = 0;
	req.msg.cmd = GET_PWR_HEADROOM_CMD;
	req.msg.data_len = 0;

	rsp = intf->sendrecv(intf, &req);
	if (!rsp) {
		lprintf(LOG_ERR, "Error getting power headroom status");
		return -1;
	} else if ((iDRAC_FLAG_12_13)
			&& (rsp->ccode == LICENSE_NOT_SUPPORTED)) {
		lprintf(LOG_ERR,
				"FM001 : A required license is missing or expired");
		return -1;
	} else if ((rsp->ccode == 0xc1) || (rsp->ccode == 0xcb)) {
		lprintf(LOG_ERR, "Error getting power headroom status: "
				"Command not supported on this system ");
		return -1;
	} else if (rsp->ccode) {
		lprintf(LOG_ERR, "Error getting power headroom status: %s",
				val2str(rsp->ccode, completion_code_vals));
		return -1;
	}
	if (verbose > 1) {
		/* need to look into */
		printf("power headroom  Data               : %x %x %x %x ", rsp->data[0],
				rsp->data[1], rsp->data[2], rsp->data[3]);
	}
	powerheadroom= *(( POWER_HEADROOM *)rsp->data);
# if WORDS_BIGENDIAN
	powerheadroom.instheadroom = BSWAP_16(powerheadroom.instheadroom);
	powerheadroom.peakheadroom = BSWAP_16(powerheadroom.peakheadroom);
# endif
	printf("Headroom\n");
	printf("Statistic                     Reading\n");
	if (unit == btuphr) {
		peakpowerheadroombtuphr = watt_to_btuphr_conversion(powerheadroom.peakheadroom);
		instantpowerhearoom = watt_to_btuphr_conversion(powerheadroom.instheadroom);
		printf("System Instantaneous Headroom : %" PRId64 " BTU/hr\n",
				instantpowerhearoom);
		printf("System Peak Headroom          : %" PRId64 " BTU/hr\n",
				peakpowerheadroombtuphr);
	} else {
		printf("System Instantaneous Headroom : %d W\n",
				powerheadroom.instheadroom);
		printf("System Peak Headroom          : %d W\n",
				powerheadroom.peakheadroom);
	}
	return 0;
}
/*
 * Function Name:       ipmi_get_power_consumption_data
 *
 * Description:         This function updates the instant Power consumption information
 * Input:               intf - ipmi interface
 * Output:              power consumption current reading
 *                      Assumption value will be in Watt.
 *
 * Return:
 */
static int
ipmi_get_power_consumption_data(struct ipmi_intf * intf,uint8_t unit)
{
	SensorReadingType sensorReadingData;

	struct ipmi_rs * rsp=NULL;
	struct sdr_record_list *sdr;
	int readingbtuphr = 0;
	int warning_threshbtuphr = 0;
	int failure_threshbtuphr = 0;
	int status = 0;
	int sensor_number = 0;
	sdr = ipmi_sdr_find_sdr_byid(intf, "System Level");
	if (!sdr) {
		lprintf(LOG_ERR,
				"Error : Can not access the System Level sensor data");
		return -1;
	}
	sensor_number = sdr->record.common->keys.sensor_num;
	ipmi_get_sensor_reading(intf,sensor_number,&sensorReadingData);
	rsp = ipmi_sdr_get_sensor_thresholds(intf,
			sdr->record.common->keys.sensor_num,
			sdr->record.common->keys.owner_id,
			sdr->record.common->keys.lun,
			sdr->record.common->keys.channel);
	if (!rsp || rsp->ccode) {
		lprintf(LOG_ERR,
				"Error : Can not access the System Level sensor data");
		return -1;
	}
	readingbtuphr = sdr_convert_sensor_reading(sdr->record.full,
			sensorReadingData.sensorReading);
	warning_threshbtuphr = sdr_convert_sensor_reading(sdr->record.full,
			rsp->data[4]);
	failure_threshbtuphr = sdr_convert_sensor_reading(sdr->record.full,
			rsp->data[5]);

	printf("System Board System Level\n");
	if (unit == btuphr) {
		readingbtuphr = watt_to_btuphr_conversion(readingbtuphr);
		warning_threshbtuphr = watt_to_btuphr_conversion(warning_threshbtuphr);
		failure_threshbtuphr = watt_to_btuphr_conversion( failure_threshbtuphr);

		printf("Reading                        : %d BTU/hr\n", readingbtuphr);
		printf("Warning threshold      : %d BTU/hr\n", warning_threshbtuphr);
		printf("Failure threshold      : %d BTU/hr\n", failure_threshbtuphr);
	} else {
		printf("Reading                        : %d W \n",readingbtuphr);
		printf("Warning threshold      : %d W \n",(warning_threshbtuphr));
		printf("Failure threshold      : %d W \n",(failure_threshbtuphr));
	}
	return status;
}
/*
 * Function Name:      ipmi_get_instan_power_consmpt_data
 *
 * Description:        This function updates the instant Power consumption information
 * Input:              intf - ipmi interface
 * Output:             instpowerconsumptiondata - instant Power consumption information
 *
 * Return:
 */
static int
ipmi_get_instan_power_consmpt_data(struct ipmi_intf * intf,
		IPMI_INST_POWER_CONSUMPTION_DATA * instpowerconsumptiondata)
{
	struct ipmi_rs * rsp;
	struct ipmi_rq req={0};
	uint8_t msg_data[2];
	/*get instantaneous power consumption command*/
	req.msg.netfn = DELL_OEM_NETFN;
	req.msg.lun = 0;
	req.msg.cmd = GET_PWR_CONSUMPTION_CMD;
	req.msg.data = msg_data;
	req.msg.data_len = 2;
	memset(msg_data, 0, 2);
	msg_data[0] = 0x0A;
	msg_data[1] = 0x00;

	rsp = intf->sendrecv(intf, &req);
	if (!rsp) {
		lprintf(LOG_ERR, "Error getting instantaneous power consumption data .");
		return -1;
	} else if ((iDRAC_FLAG_12_13)
			&& (rsp->ccode == LICENSE_NOT_SUPPORTED)) {
		lprintf(LOG_ERR,
				"FM001 : A required license is missing or expired");
		return -1;
	} else if ((rsp->ccode == 0xc1) || (rsp->ccode == 0xcb)) {
		lprintf(LOG_ERR, "Error getting instantaneous power consumption data: "
				"Command not supported on this system.");
		return -1;
	} else if (rsp->ccode) {
		lprintf(LOG_ERR, "Error getting instantaneous power consumption data: %s",
				val2str(rsp->ccode, completion_code_vals));
		return -1;
	}
	*instpowerconsumptiondata = *((IPMI_INST_POWER_CONSUMPTION_DATA *)(rsp->data));
#if WORDS_BIGENDIAN
	instpowerconsumptiondata->instanpowerconsumption = BSWAP_16(instpowerconsumptiondata->instanpowerconsumption);
	instpowerconsumptiondata->instanApms = BSWAP_16(instpowerconsumptiondata->instanApms);
	instpowerconsumptiondata->resv1 = BSWAP_16(instpowerconsumptiondata->resv1);
#endif
	return 0;
}
/*
 * Function Name:      ipmi_print_get_instan_power_Amps_data
 *
 * Description:        This function prints the instant Power consumption information
 * Input:              instpowerconsumptiondata - instant Power consumption information
 * Output:
 *
 * Return:
 */
static void
ipmi_print_get_instan_power_Amps_data(IPMI_INST_POWER_CONSUMPTION_DATA instpowerconsumptiondata)
{
	uint16_t intampsval=0;
	uint16_t decimalampsval=0;
	if (instpowerconsumptiondata.instanApms > 0) {
		decimalampsval = (instpowerconsumptiondata.instanApms % 10);
		intampsval = instpowerconsumptiondata.instanApms / 10;
	}
	printf("\nAmperage value: %d.%d A \n", intampsval, decimalampsval);
}
/*
 * Function Name:     ipmi_print_get_power_consmpt_data
 *
 * Description:       This function prints the Power consumption information
 * Input:             intf            - ipmi interface
 *                    unit            - watt / btuphr
 * Output:
 *
 * Return:
 */
static int
ipmi_print_get_power_consmpt_data(struct ipmi_intf * intf, uint8_t unit)
{
	int rc = 0;
	IPMI_INST_POWER_CONSUMPTION_DATA instpowerconsumptiondata = {0,0,0,0};
	printf("\nPower consumption information\n");
	rc = ipmi_get_power_consumption_data(intf, unit);
	if (rc == (-1)) {
		return rc;
	}
	rc = ipmi_get_instan_power_consmpt_data(intf, &instpowerconsumptiondata);
	if (rc == (-1)) {
		return rc;
	}
	ipmi_print_get_instan_power_Amps_data(instpowerconsumptiondata);
	rc = ipmi_get_power_headroom_command(intf, unit);
	if (rc == (-1)) {
		return rc;
	}
	return rc;
}
/*
 * Function Name:   ipmi_get_avgpower_consmpt_history
 *
 * Description:     This function updates the average power consumption information
 * Input:           intf            - ipmi interface
 * Output:          pavgpower- average power consumption information
 *
 * Return:
 */
static int
ipmi_get_avgpower_consmpt_history(struct ipmi_intf * intf,
		IPMI_AVGPOWER_CONSUMP_HISTORY * pavgpower)
{
	int rc;
	uint8_t *rdata;
	rc = ipmi_mc_getsysinfo(intf, 0xeb, 0, 0, sizeof(*pavgpower), pavgpower);
	if (rc < 0) {
		lprintf(LOG_ERR,
				"Error getting average power consumption history data.");
		return -1;
	} else if ((iDRAC_FLAG_12_13) &&  (rc == LICENSE_NOT_SUPPORTED)) {
		lprintf(LOG_ERR,
				"FM001 : A required license is missing or expired");
		return -1;
	} else if ((rc == 0xc1) || (rc == 0xcb)) {
		lprintf(LOG_ERR, "Error getting average power consumption history data: "
				"Command not supported on this system.");
		return -1;
	} else if (rc != 0) {
		lprintf(LOG_ERR,
				"Error getting average power consumption history data: %s",
				val2str(rc, completion_code_vals));
		return -1;
	}
	if (verbose > 1) {
		rdata = (void *)pavgpower;
		printf("Average power consumption history data"
				"       :%x %x %x %x %x %x %x %x\n\n",
				rdata[0], rdata[1], rdata[2], rdata[3],
				rdata[4], rdata[5], rdata[6], rdata[7]);
	}
# if WORDS_BIGENDIAN
	pavgpower->lastminutepower = BSWAP_16(pavgpower->lastminutepower);
	pavgpower->lasthourpower = BSWAP_16(pavgpower->lasthourpower);
	pavgpower->lastdaypower = BSWAP_16(pavgpower->lastdaypower);
	pavgpower->lastweakpower = BSWAP_16(pavgpower->lastweakpower);
# endif
	return 0;
}
/*
 * Function Name:    ipmi_get_peakpower_consmpt_history
 *
 * Description:      This function updates the peak power consumption information
 * Input:            intf            - ipmi interface
 * Output:           pavgpower- peak power consumption information
 *
 * Return:
 */
static int
ipmi_get_peakpower_consmpt_history(struct ipmi_intf * intf,
		IPMI_POWER_CONSUMP_HISTORY * pstPeakpower)
{
	uint8_t *rdata;
	int rc;
	rc = ipmi_mc_getsysinfo(intf, 0xec, 0, 0, sizeof(*pstPeakpower),
			pstPeakpower);
	if (rc < 0) {
		lprintf(LOG_ERR, "Error getting  peak power consumption history data.");
		return -1;
	} else if ((iDRAC_FLAG_12_13) && (rc == LICENSE_NOT_SUPPORTED)) {
		lprintf(LOG_ERR,
				"FM001 : A required license is missing or expired");
		return -1;
	} else if ((rc == 0xc1) || (rc == 0xcb)) {
		lprintf(LOG_ERR, "Error getting peak power consumption history data: "
				"Command not supported on this system.");
		return -1;
	} else if (rc != 0) {
		lprintf(LOG_ERR, "Error getting peak power consumption history data: %s",
				val2str(rc, completion_code_vals));
		return -1;
	}
	if (verbose > 1) {
		rdata = (void *)pstPeakpower;
		printf("Peak power consmhistory  Data               : "
				"%x %x %x %x %x %x %x %x %x %x\n   "
				"%x %x %x %x %x %x %x %x %x %x %x %x %x %x\n\n",
				rdata[0], rdata[1], rdata[2], rdata[3],
				rdata[4], rdata[5], rdata[6], rdata[7],
				rdata[8], rdata[9], rdata[10], rdata[11],
				rdata[12], rdata[13], rdata[14], rdata[15],
				rdata[16], rdata[17], rdata[18], rdata[19],
				rdata[20], rdata[21], rdata[22], rdata[23]);
	}
# if WORDS_BIGENDIAN
	pstPeakpower->lastminutepower = BSWAP_16(pstPeakpower->lastminutepower);
	pstPeakpower->lasthourpower = BSWAP_16(pstPeakpower->lasthourpower);
	pstPeakpower->lastdaypower = BSWAP_16(pstPeakpower->lastdaypower);
	pstPeakpower->lastweakpower = BSWAP_16(pstPeakpower->lastweakpower);
	pstPeakpower->lastminutepowertime = BSWAP_32(pstPeakpower->lastminutepowertime);
	pstPeakpower->lasthourpowertime = BSWAP_32(pstPeakpower->lasthourpowertime);
	pstPeakpower->lastdaypowertime = BSWAP_32(pstPeakpower->lastdaypowertime);
	pstPeakpower->lastweekpowertime = BSWAP_32(pstPeakpower->lastweekpowertime);
#endif
	return 0;
}
/*
 * Function Name:    ipmi_get_minpower_consmpt_history
 *
 * Description:      This function updates the peak power consumption information
 * Input:            intf            - ipmi interface
 * Output:           pavgpower- peak power consumption information
 *
 * Return:
 */
static int
ipmi_get_minpower_consmpt_history(struct ipmi_intf * intf,
		IPMI_POWER_CONSUMP_HISTORY * pstMinpower)
{
	uint8_t *rdata;
	int rc;
	rc = ipmi_mc_getsysinfo(intf, 0xed, 0, 0, sizeof(*pstMinpower),
			pstMinpower);
	if (rc < 0) {
		lprintf(LOG_ERR, "Error getting  peak power consumption history data .");
		return -1;
	} else if ((iDRAC_FLAG_12_13) &&  (rc == LICENSE_NOT_SUPPORTED)) {
		lprintf(LOG_ERR,
				"FM001 : A required license is missing or expired");
		return -1;
	} else if ((rc == 0xc1) || (rc == 0xcb)) {
		lprintf(LOG_ERR, "Error getting peak power consumption history data: "
				"Command not supported on this system.");
		return -1;
	} else if (rc != 0) {
		lprintf(LOG_ERR, "Error getting peak power consumption history data: %s",
				val2str(rc, completion_code_vals));
		return -1;
	}
	if (verbose > 1) {
		rdata = (void *)pstMinpower;
		printf("Peak power consmhistory  Data               : "
				"%x %x %x %x %x %x %x %x %x %x\n   "
				"%x %x %x %x %x %x %x %x %x %x %x %x %x\n\n",
				rdata[0], rdata[1], rdata[2], rdata[3],
				rdata[4], rdata[5], rdata[6], rdata[7],
				rdata[8], rdata[9], rdata[10], rdata[11],
				rdata[12], rdata[13], rdata[14], rdata[15],
				rdata[16], rdata[17], rdata[18], rdata[19],
				rdata[20], rdata[21], rdata[22], rdata[23]);
	}
# if WORDS_BIGENDIAN
	pstMinpower->lastminutepower = BSWAP_16(pstMinpower->lastminutepower);
	pstMinpower->lasthourpower = BSWAP_16(pstMinpower->lasthourpower);
	pstMinpower->lastdaypower = BSWAP_16(pstMinpower->lastdaypower);
	pstMinpower->lastweakpower = BSWAP_16(pstMinpower->lastweakpower);
	pstMinpower->lastminutepowertime = BSWAP_32(pstMinpower->lastminutepowertime);
	pstMinpower->lasthourpowertime = BSWAP_32(pstMinpower->lasthourpowertime);
	pstMinpower->lastdaypowertime = BSWAP_32(pstMinpower->lastdaypowertime);
	pstMinpower->lastweekpowertime = BSWAP_32(pstMinpower->lastweekpowertime);
# endif
	return 0;
}
/*
 * Function Name:    ipmi_print_power_consmpt_history
 *
 * Description:      This function print the average and peak power consumption information
 * Input:            intf      - ipmi interface
 *                   unit      - watt / btuphr
 * Output:
 *
 * Return:
 */
static int
ipmi_print_power_consmpt_history(struct ipmi_intf * intf, int unit)
{
	uint64_t tmp;
	int rc = 0;

	IPMI_AVGPOWER_CONSUMP_HISTORY avgpower;
	IPMI_POWER_CONSUMP_HISTORY stMinpower;
	IPMI_POWER_CONSUMP_HISTORY stPeakpower;

	rc = ipmi_get_avgpower_consmpt_history(intf, &avgpower);
	if (rc == (-1)) {
		return rc;
	}

	rc = ipmi_get_peakpower_consmpt_history(intf, &stPeakpower);
	if (rc == (-1)) {
		return rc;
	}

	rc = ipmi_get_minpower_consmpt_history(intf, &stMinpower);
	if (rc == (-1)) {
		return rc;
	}
	if (rc != 0) {
		return rc;
	}
	printf("Power Consumption History\n\n");
	/* The fields are aligned manually changing the spaces will alter
	 * the alignment*/
	printf("Statistic                   Last Minute     Last Hour     "
			"Last Day     Last Week\n\n");
	if (unit == btuphr) {
		printf("Average Power Consumption  ");
		tmp = watt_to_btuphr_conversion(avgpower.lastminutepower);
		printf("%4" PRId64 " BTU/hr     ", tmp);
		tmp = watt_to_btuphr_conversion(avgpower.lasthourpower);
		printf("%4" PRId64 " BTU/hr   ", tmp);
		tmp = watt_to_btuphr_conversion(avgpower.lastdaypower);
		printf("%4" PRId64 " BTU/hr  ", tmp);
		tmp = watt_to_btuphr_conversion(avgpower.lastweakpower);
		printf("%4" PRId64 " BTU/hr\n", tmp);

		printf("Max Power Consumption      ");
		tmp = watt_to_btuphr_conversion(stPeakpower.lastminutepower);
		printf("%4" PRId64 " BTU/hr     ", tmp);
		tmp = watt_to_btuphr_conversion(stPeakpower.lasthourpower);
		printf("%4" PRId64 " BTU/hr   ", tmp);
		tmp = watt_to_btuphr_conversion(stPeakpower.lastdaypower);
		printf("%4" PRId64 " BTU/hr  ", tmp);
		tmp = watt_to_btuphr_conversion(stPeakpower.lastweakpower);
		printf("%4" PRId64 " BTU/hr\n", tmp);

		printf("Min Power Consumption      ");
		tmp = watt_to_btuphr_conversion(stMinpower.lastminutepower);
		printf("%4" PRId64 " BTU/hr     ", tmp);
		tmp = watt_to_btuphr_conversion(stMinpower.lasthourpower);
		printf("%4" PRId64 " BTU/hr   ", tmp);
		tmp = watt_to_btuphr_conversion(stMinpower.lastdaypower);
		printf("%4" PRId64 " BTU/hr  ", tmp);
		tmp = watt_to_btuphr_conversion(stMinpower.lastweakpower);
		printf("%4" PRId64 " BTU/hr\n\n", tmp);
	} else {
		printf("Average Power Consumption  ");
		tmp = avgpower.lastminutepower;
		printf("%4" PRId64 " W          ", tmp);
		tmp = avgpower.lasthourpower;
		printf("%4" PRId64 " W        ", tmp);
		tmp = avgpower.lastdaypower;
		printf("%4" PRId64 " W       ", tmp);
		tmp = avgpower.lastweakpower;
		printf("%4" PRId64 " W   \n", tmp);

		printf("Max Power Consumption      ");
		tmp = stPeakpower.lastminutepower;
		printf("%4" PRId64 " W          ", tmp);
		tmp = stPeakpower.lasthourpower;
		printf("%4" PRId64 " W        ", tmp);
		tmp = stPeakpower.lastdaypower;
		printf("%4" PRId64 " W       ", tmp);
		tmp = stPeakpower.lastweakpower;
		printf("%4" PRId64 " W   \n", tmp);

		printf("Min Power Consumption      ");
		tmp = stMinpower.lastminutepower;
		printf("%4" PRId64 " W          ", tmp);
		tmp = stMinpower.lasthourpower;
		printf("%4" PRId64 " W        ", tmp);
		tmp = stMinpower.lastdaypower;
		printf("%4" PRId64 " W       ", tmp);
		tmp = stMinpower.lastweakpower;
		printf("%4" PRId64 " W   \n\n", tmp);
	}

	printf("Max Power Time\n");
	printf("Last Minute     : %s",
	       ipmi_timestamp_numeric(stPeakpower.lastminutepowertime));
	printf("Last Hour       : %s",
	       ipmi_timestamp_numeric(stPeakpower.lasthourpowertime));
	printf("Last Day        : %s",
	       ipmi_timestamp_numeric(stPeakpower.lastdaypowertime));
	printf("Last Week       : %s",
	       ipmi_timestamp_numeric(stPeakpower.lastweekpowertime));

	printf("Min Power Time\n");
	printf("Last Minute     : %s",
	       ipmi_timestamp_numeric(stMinpower.lastminutepowertime));
	printf("Last Hour       : %s",
	       ipmi_timestamp_numeric(stMinpower.lasthourpowertime));
	printf("Last Day        : %s",
	       ipmi_timestamp_numeric(stMinpower.lastdaypowertime));
	printf("Last Week       : %s",
	       ipmi_timestamp_numeric(stMinpower.lastweekpowertime));
	return rc;
}
/*
 * Function Name:    ipmi_get_power_cap
 *
 * Description:      This function updates the power cap information
 * Input:            intf         - ipmi interface
 * Output:           ipmipowercap - power cap information
 *
 * Return:
 */
static int
ipmi_get_power_cap(struct ipmi_intf * intf, IPMI_POWER_CAP * ipmipowercap)
{
	uint8_t *rdata;
	int rc;
	rc = ipmi_mc_getsysinfo(intf, IPMI_DELL_POWER_CAP, 0, 0,
			sizeof(*ipmipowercap), ipmipowercap);
	if (rc < 0) {
		lprintf(LOG_ERR, "Error getting power cap.");
		return -1;
	} else if ((iDRAC_FLAG_12_13) && (rc == LICENSE_NOT_SUPPORTED)) {
		lprintf(LOG_ERR,
				"FM001 : A required license is missing or expired");
		return -1;
	} else if ((rc == 0xc1) || (rc == 0xcb)) {
		lprintf(LOG_ERR, "Error getting power cap: "
				"Command not supported on this system.");
		return -1;
	} else if (rc != 0) {
		lprintf(LOG_ERR, "Error getting power cap: %s",
				val2str(rc, completion_code_vals));
		return -1;
	}
	if (verbose > 1) {
		rdata = (void*)ipmipowercap;
		printf("power cap  Data               :%x %x %x %x %x %x %x %x %x %x ",
				rdata[1], rdata[2], rdata[3],
				rdata[4], rdata[5], rdata[6], rdata[7],
				rdata[8], rdata[9], rdata[10],rdata[11]);
	}
# if WORDS_BIGENDIAN
	ipmipowercap->PowerCap = BSWAP_16(ipmipowercap->PowerCap);
	ipmipowercap->MaximumPowerConsmp = BSWAP_16(ipmipowercap->MaximumPowerConsmp);
	ipmipowercap->MinimumPowerConsmp = BSWAP_16(ipmipowercap->MinimumPowerConsmp);
	ipmipowercap->totalnumpowersupp = BSWAP_16(ipmipowercap->totalnumpowersupp);
	ipmipowercap->AvailablePower = BSWAP_16(ipmipowercap->AvailablePower);
	ipmipowercap->SystemThrottling = BSWAP_16(ipmipowercap->SystemThrottling);
	ipmipowercap->Resv = BSWAP_16(ipmipowercap->Resv);
# endif
	return 0;
}
/*
 * Function Name:    ipmi_print_power_cap
 *
 * Description:      This function print the power cap information
 * Input:            intf            - ipmi interface
 *                   unit            - watt / btuphr
 * Output:
 * Return:
 */
static int
ipmi_print_power_cap(struct ipmi_intf * intf,uint8_t unit)
{
	uint64_t tempbtuphrconv;
	int rc;
	IPMI_POWER_CAP ipmipowercap;
	memset(&ipmipowercap, 0, sizeof(ipmipowercap));
	rc = ipmi_get_power_cap(intf, &ipmipowercap);
	if (rc == 0) {
		if (unit == btuphr) {
			tempbtuphrconv = watt_to_btuphr_conversion(ipmipowercap.MaximumPowerConsmp);
			printf("Maximum power: %" PRId64 "  BTU/hr\n", tempbtuphrconv);
			tempbtuphrconv = watt_to_btuphr_conversion(ipmipowercap.MinimumPowerConsmp);
			printf("Minimum power: %" PRId64 "  BTU/hr\n", tempbtuphrconv);
			tempbtuphrconv = watt_to_btuphr_conversion(ipmipowercap.PowerCap);
			printf("Power cap    : %" PRId64 "  BTU/hr\n", tempbtuphrconv);
		} else {
			printf("Maximum power: %d Watt\n", ipmipowercap.MaximumPowerConsmp);
			printf("Minimum power: %d Watt\n", ipmipowercap.MinimumPowerConsmp);
			printf("Power cap    : %d Watt\n", ipmipowercap.PowerCap);
		}
	}
	return rc;
}
/*
 * Function Name:     ipmi_set_power_cap
 *
 * Description:       This function updates the power cap information
 * Input:             intf            - ipmi interface
 *                    unit            - watt / btuphr
 *                    val             - new power cap value
 * Output:
 * Return:
 */
static int
ipmi_set_power_cap(struct ipmi_intf * intf, int unit, int val)
{
	int rc;
	uint8_t data[13], *rdata;
	uint16_t powercapval;
	uint64_t maxpowerbtuphr;
	uint64_t minpowerbtuphr;
	IPMI_POWER_CAP ipmipowercap;

	if (ipmi_get_power_capstatus_command(intf) < 0) {
		return -1; /* Adding the failed condition check */
	}
	if (PowercapSetable_flag != 1) {
		lprintf(LOG_ERR, "Can not set powercap on this system");
		return -1;
	} else if (PowercapstatusFlag != 1) {
		lprintf(LOG_ERR, "Power cap set feature is not enabled");
		return -1;
	}
	rc = ipmi_mc_getsysinfo(intf, IPMI_DELL_POWER_CAP, 0, 0,
			sizeof(ipmipowercap), &ipmipowercap);
	if (rc < 0) {
		lprintf(LOG_ERR, "Error getting power cap.");
		return -1;
	} else if ((iDRAC_FLAG_12_13) && (rc == LICENSE_NOT_SUPPORTED)) {
		lprintf(LOG_ERR,
				"FM001 : A required license is missing or expired");
		return -1;
	} else if (rc == 0xc1) {
		lprintf(LOG_ERR, "Error getting power cap, command not supported on "
				"this system.");
		return -1;
	} else if (rc != 0) {
		lprintf(LOG_ERR, "Error getting power cap: %s",
				val2str(rc, completion_code_vals));
		return -1;
	}
	if (verbose > 1) {
		rdata = (void *)&ipmipowercap;
		printf("power cap  Data               :%x %x %x %x %x %x %x %x %x %x %x ",
				rdata[1], rdata[2], rdata[3],
				rdata[4], rdata[5], rdata[6], rdata[7],
				rdata[8], rdata[9], rdata[10],rdata[11]);
	}
# if WORDS_BIGENDIAN
	ipmipowercap.PowerCap = BSWAP_16(ipmipowercap.PowerCap);
	ipmipowercap.MaximumPowerConsmp = BSWAP_16(ipmipowercap.MaximumPowerConsmp);
	ipmipowercap.MinimumPowerConsmp = BSWAP_16(ipmipowercap.MinimumPowerConsmp);
	ipmipowercap.AvailablePower = BSWAP_16(ipmipowercap.AvailablePower);
	ipmipowercap.totalnumpowersupp = BSWAP_16(ipmipowercap.totalnumpowersupp);
# endif
	memset(data, 0, 13);
	data[0] = IPMI_DELL_POWER_CAP;
	powercapval = val;
	data[1] = (powercapval & 0XFF);
	data[2] = ((powercapval & 0XFF00) >> 8);
	data[3] = unit;
	data[4] = ((ipmipowercap.MaximumPowerConsmp & 0xFF));
	data[5] = ((ipmipowercap.MaximumPowerConsmp & 0xFF00) >> 8);
	data[6] = ((ipmipowercap.MinimumPowerConsmp & 0xFF));
	data[7] = ((ipmipowercap.MinimumPowerConsmp & 0xFF00) >> 8);
	data[8] = (ipmipowercap.totalnumpowersupp);
	data[9] = ((ipmipowercap.AvailablePower & 0xFF));
	data[10] = ((ipmipowercap.AvailablePower & 0xFF00) >> 8);
	data[11] = (ipmipowercap.SystemThrottling);
	data[12] = 0x00;

	if (unit == btuphr) {
		val = btuphr_to_watt_conversion(val);
	} else if (unit == percent) {
		if ((val < 0) || (val > 100)) {
			lprintf(LOG_ERR, "Cap value is out of boundary condition it "
					"should be between 0  - 100");
			return -1;
		}
		val = ((val*(ipmipowercap.MaximumPowerConsmp
						- ipmipowercap.MinimumPowerConsmp)) / 100)
						+ ipmipowercap.MinimumPowerConsmp;
		lprintf(LOG_ERR, "Cap value in percentage is  %d ", val);
		data[1] = (val & 0XFF);
		data[2] = ((val & 0XFF00) >> 8);
		data[3] = watt;
	}
	if (((val < ipmipowercap.MinimumPowerConsmp)
				|| (val > ipmipowercap.MaximumPowerConsmp)) && (unit == watt)) {
		lprintf(LOG_ERR,
				"Cap value is out of boundary condition it should be between %d  - %d",
				ipmipowercap.MinimumPowerConsmp, ipmipowercap.MaximumPowerConsmp);
		return -1;
	} else if (((val < ipmipowercap.MinimumPowerConsmp)
				|| (val > ipmipowercap.MaximumPowerConsmp)) && (unit == btuphr)) {
		minpowerbtuphr = watt_to_btuphr_conversion(ipmipowercap.MinimumPowerConsmp);
		maxpowerbtuphr = watt_to_btuphr_conversion(ipmipowercap.MaximumPowerConsmp);
		lprintf(LOG_ERR,
				"Cap value is out of boundary condition it should be between %d",
				minpowerbtuphr);
		lprintf(LOG_ERR, " -%d", maxpowerbtuphr);
		return -1;
	}
	rc = ipmi_mc_setsysinfo(intf, 13, data);
	if (rc < 0) {
		lprintf(LOG_ERR, "Error setting power cap");
		return -1;
	} else if ((iDRAC_FLAG_12_13) && (rc == LICENSE_NOT_SUPPORTED)) {
		lprintf(LOG_ERR,
				"FM001 : A required license is missing or expired");
		return -1;
	} else if (rc > 0) {
		lprintf(LOG_ERR, "Error setting power cap: %s",
				val2str(rc, completion_code_vals));
		return -1;
	}
	if (verbose > 1) {
		printf("CC for setpowercap :%d ", rc);
	}
	return 0;
}
/*
 * Function Name:   ipmi_powermonitor_usage
 *
 * Description:     This function prints help message for powermonitor command
 * Input:
 * Output:
 *
 * Return:
 */
static void
ipmi_powermonitor_usage(void)
{
	lprintf(LOG_NOTICE,
"");
	lprintf(LOG_NOTICE,
"   powermonitor");
	lprintf(LOG_NOTICE,
"      Shows power tracking statistics ");
	lprintf(LOG_NOTICE,
"");
	lprintf(LOG_NOTICE,
"   powermonitor clear cumulativepower");
	lprintf(LOG_NOTICE,
"      Reset cumulative power reading");
	lprintf(LOG_NOTICE,
"");
	lprintf(LOG_NOTICE,
"   powermonitor clear peakpower");
	lprintf(LOG_NOTICE,
"      Reset peak power reading");
	lprintf(LOG_NOTICE,
"");
	lprintf(LOG_NOTICE,
"   powermonitor powerconsumption");
	lprintf(LOG_NOTICE,
"      Displays power consumption in <watt|btuphr>");
	lprintf(LOG_NOTICE,
"");
	lprintf(LOG_NOTICE,
"   powermonitor powerconsumptionhistory <watt|btuphr>");
	lprintf(LOG_NOTICE,
"      Displays power consumption history ");
	lprintf(LOG_NOTICE,
"");
	lprintf(LOG_NOTICE,
"   powermonitor getpowerbudget");
	lprintf(LOG_NOTICE,
"      Displays power cap in <watt|btuphr>");
	lprintf(LOG_NOTICE,
"");
	lprintf(LOG_NOTICE,
"   powermonitor setpowerbudget <val><watt|btuphr|percent>");
	lprintf(LOG_NOTICE,
"      Allows user to set the  power cap in <watt|BTU/hr|percentage>");
	lprintf(LOG_NOTICE,
"");
	lprintf(LOG_NOTICE,
"   powermonitor enablepowercap ");
	lprintf(LOG_NOTICE,
"      To enable set power cap");
	lprintf(LOG_NOTICE,
"");
	lprintf(LOG_NOTICE,
"   powermonitor disablepowercap ");
	lprintf(LOG_NOTICE,
"      To disable set power cap");
	lprintf(LOG_NOTICE,
"");
}
/*
 * Function Name:	   ipmi_delloem_vFlash_main
 *
 * Description:		   This function processes the delloem vFlash command
 * Input:			   intf    - ipmi interface
 *					   argc    - no of arguments (unused)
 *					   argv    - argument string array
 * Output:
 *
 * Return:			   return code	   0 - success
 *						  -1 - failure
 */
static int
ipmi_delloem_vFlash_main(struct ipmi_intf * intf, int __UNUSED__(argc), char ** argv)
{
	int rc = 0;
	current_arg++;
	rc = ipmi_delloem_vFlash_process(intf, current_arg, argv);
	return rc;
}
/*
 * Function Name: 	get_vFlash_compcode_str
 *
 * Description: 	This function maps the vFlash completion code
 * 		to a string
 * Input : vFlash completion code and static array of codes vs strings
 * Output: -
 * Return: returns the mapped string
 */
const char *
get_vFlash_compcode_str(uint8_t vflashcompcode, const struct vFlashstr *vs)
{
	static char un_str[32];
	int i;
	for (i = 0; vs[i].str; i++) {
		if (vs[i].val == vflashcompcode)
			return vs[i].str;
	}
	memset(un_str, 0, 32);
	snprintf(un_str, 32, "Unknown (0x%02X)", vflashcompcode);
	return un_str;
}
/*
 * Function Name: 	ipmi_get_sd_card_info
 *
 * Description: This function prints the vFlash Extended SD card info
 * Input : ipmi interface
 * Output: prints the sd card extended info
 * Return: 0 - success -1 - failure
 */
static int
ipmi_get_sd_card_info(struct ipmi_intf * intf) {
	struct ipmi_rs * rsp;
	struct ipmi_rq req;
	uint8_t msg_data[2];
	uint8_t input_length=0;
	uint8_t cardstatus=0x00;
	IPMI_DELL_SDCARD_INFO * sdcardinfoblock;

	input_length = 2;
	msg_data[0] = msg_data[1] = 0x00;
	req.msg.netfn = DELL_OEM_NETFN;
	req.msg.lun = 0;
	req.msg.cmd = IPMI_GET_EXT_SD_CARD_INFO;
	req.msg.data = msg_data;
	req.msg.data_len = input_length;

	rsp = intf->sendrecv(intf, &req);
	if (!rsp) {
		lprintf(LOG_ERR, "Error in getting SD Card Extended Information");
		return -1;
	} else if (rsp->ccode) {
		lprintf(LOG_ERR, "Error in getting SD Card Extended Information (%s)",
				val2str(rsp->ccode, completion_code_vals));
		return -1;
	}

	sdcardinfoblock = (IPMI_DELL_SDCARD_INFO *) (void *) rsp->data;

	if ((iDRAC_FLAG_12_13)
			&& (sdcardinfoblock->vflashcompcode == VFL_NOT_LICENSED)) {
		lprintf(LOG_ERR,
				"FM001 : A required license is missing or expired");
		return -1;
	} else if (sdcardinfoblock->vflashcompcode != 0x00) {
		lprintf(LOG_ERR, "Error in getting SD Card Extended Information (%s)",
				get_vFlash_compcode_str(sdcardinfoblock->vflashcompcode,
					vFlash_completion_code_vals));
		return -1;
	}

	if (!(sdcardinfoblock->sdcardstatus & 0x04)) {
		lprintf(LOG_ERR,
				"vFlash SD card is unavailable, please insert the card of");
		lprintf(LOG_ERR,
				"size 256MB or greater");
		return (-1);
	}

	printf("vFlash SD Card Properties\n");
	printf("SD Card size       : %8dMB\n", sdcardinfoblock->sdcardsize);
	printf("Available size     : %8dMB\n", sdcardinfoblock->sdcardavailsize);
	printf("Initialized        : %10s\n",
			(sdcardinfoblock->sdcardstatus & 0x80) ? "Yes" : "No");
	printf("Licensed           : %10s\n",
			(sdcardinfoblock->sdcardstatus & 0x40) ? "Yes" : "No");
	printf("Attached           : %10s\n",
			(sdcardinfoblock->sdcardstatus & 0x20) ? "Yes" : "No");
	printf("Enabled            : %10s\n",
			(sdcardinfoblock->sdcardstatus & 0x10) ? "Yes" : "No");
	printf("Write Protected    : %10s\n",
			(sdcardinfoblock->sdcardstatus & 0x08) ? "Yes" : "No");
	cardstatus = sdcardinfoblock->sdcardstatus & 0x03;
	printf("Health             : %10s\n",
			((0x00 == cardstatus) ? "OK" : (
				(cardstatus == 0x03) ? "Undefined" : (
					(cardstatus == 0x02) ? "Critical" : "Warning"))));
	printf("Bootable partition : %10d\n", sdcardinfoblock->bootpartion);
	return 0;
}
/*
 * Function Name: 	ipmi_delloem_vFlash_process
 *
 * Description: 	This function processes the args for vFlash subcmd
 * Input : intf - ipmi interface, arg index, argv array
 * Output: prints help or error with help
 * Return: 0 - Success -1 - failure
 */
static int
ipmi_delloem_vFlash_process(struct ipmi_intf * intf, int current_arg, char ** argv)
{
	int rc;
	if (strcmp(intf->name,"wmi") && strcmp(intf->name, "open")) {
		lprintf(LOG_ERR,
				"vFlash support is enabled only for wmi and open interface.");
		lprintf(LOG_ERR, "Its not enabled for lan and lanplus interface.");
		return -1;
	}

	if (!argv[current_arg] || !strcmp(argv[current_arg], "help")) {
		ipmi_vFlash_usage();
		return 0;
	}
	ipmi_idracvalidator_command(intf);
	if (!strcmp(argv[current_arg], "info")) {
		current_arg++;
		if (!argv[current_arg]) {
			ipmi_vFlash_usage();
			return -1;
		} else if (!strcmp(argv[current_arg], "Card")) {
			current_arg++;
			if (argv[current_arg]) {
				ipmi_vFlash_usage();
				return -1;
			}
			rc = ipmi_get_sd_card_info(intf);
			return rc;
		} else {
			/* TBD: many sub commands are present */
			ipmi_vFlash_usage();
			return -1;
		}
	} else {
		/* TBD other vFlash subcommands */
		ipmi_vFlash_usage();
		return -1;
	}
}
/*
 * Function Name: 	ipmi_vFlash_usage
 *
 * Description: 	This function displays the usage for using vFlash
 * Input : void
 * Output: prints help
 * Return: void
 */
static void
ipmi_vFlash_usage(void)
{
	lprintf(LOG_NOTICE,
"");
	lprintf(LOG_NOTICE,
"   vFlash info Card");
	lprintf(LOG_NOTICE,
"      Shows Extended SD Card information");
	lprintf(LOG_NOTICE,
"");
}
/*
 * Function Name: ipmi_setled_usage
 *
 * Description:  This function prints help message for setled command
 * Input:
 * Output:
 *
 * Return:
 */
static void
ipmi_setled_usage(void)
{
	lprintf(LOG_NOTICE,
"");
	lprintf(LOG_NOTICE,
"   setled <b:d.f> <state..>");
	lprintf(LOG_NOTICE,
"      Set backplane LED state");
	lprintf(LOG_NOTICE,
"      b:d.f = PCI Bus:Device.Function of drive (lspci format)");
	lprintf(LOG_NOTICE,
"      state = present|online|hotspare|identify|rebuilding|");
	lprintf(LOG_NOTICE,
"              fault|predict|critical|failed");
	lprintf(LOG_NOTICE,
"");
}

static int
IsSetLEDSupported(void)
{
	return SetLEDSupported;
}

static void
CheckSetLEDSupport(struct ipmi_intf * intf)
{
	struct ipmi_rs * rsp = NULL;
	struct ipmi_rq req = {0};
	uint8_t data[10];

	SetLEDSupported = 0;
	req.msg.netfn = DELL_OEM_NETFN;
	req.msg.lun = 0;
	req.msg.cmd = 0xD5; /* Storage */
	req.msg.data_len = 10;
	req.msg.data = data;

	memset(data, 0, sizeof(data));
	data[0] = 0x01; /* get */
	data[1] = 0x00; /* subcmd:get firmware version */
	data[2] = 0x08; /* length lsb */
	data[3] = 0x00; /* length msb */
	data[4] = 0x00; /* offset lsb */
	data[5] = 0x00; /* offset msb */
	data[6] = 0x00; /* bay id */
	data[7] = 0x00;
	data[8] = 0x00;
	data[9] = 0x00;

	rsp = intf->sendrecv(intf, &req);
	if (!rsp || rsp->ccode) {
		return;
	}
	SetLEDSupported = 1;
}
/*
 * Function Name:    ipmi_getdrivemap
 *
 * Description:      This function returns mapping of BDF to Bay:Slot
 * Input:            intf         - ipmi interface
 *		    bdf	 	 - PCI Address of drive
 *		    *bay	 - Returns bay ID
 *		    *slot	 - Returns slot ID
 * Output:
 *
 * Return:
 */
static int
ipmi_getdrivemap(struct ipmi_intf * intf, int b, int d, int f, int *bay,
		int *slot)
{
	struct ipmi_rs * rsp = NULL;
	struct ipmi_rq req = {0};
	uint8_t data[8];
	/* Get mapping of BDF to bay:slot */
	req.msg.netfn = DELL_OEM_NETFN;
	req.msg.lun = 0;
	req.msg.cmd = 0xD5;
	req.msg.data_len = 8;
	req.msg.data = data;

	memset(data, 0, sizeof(data));
	data[0] = 0x01; /* get */
	data[1] = 0x07; /* storage map */
	data[2] = 0x06; /* length lsb */
	data[3] = 0x00; /* length msb */
	data[4] = 0x00; /* offset lsb */
	data[5] = 0x00; /* offset msb */
	data[6] = b; /* bus */
	data[7] = (d << 3) + f; /* devfn */

	rsp = intf->sendrecv(intf, &req);
	if (!rsp) {
		lprintf(LOG_ERR, "Error issuing getdrivemap command.");
		return -1;
	} else if (rsp->ccode) {
		lprintf(LOG_ERR, "Error issuing getdrivemap command: %s",
				val2str(rsp->ccode, completion_code_vals));
		return -1;
	}
	*bay = rsp->data[7];
	*slot = rsp->data[8];
	if (*bay == 0xFF || *slot == 0xFF) {
		lprintf(LOG_ERR, "Error could not get drive bay:slot mapping");
		return -1;
	}
	return 0;
}
/*
 * Function Name:    ipmi_setled_state
 *
 * Description:      This function updates the LED on the backplane
 * Input:            intf         - ipmi interface
 *		    bdf	 	 - PCI Address of drive
 *		    state	 - SES Flags state of drive
 * Output:
 *
 * Return:
 */
static int
ipmi_setled_state(struct ipmi_intf * intf, int bayId, int slotId, int state)
{
	struct ipmi_rs * rsp = NULL;
	struct ipmi_rq req = {0};
	uint8_t data[20];
	/* Issue Drive Status Update to bay:slot */
	req.msg.netfn = DELL_OEM_NETFN;
	req.msg.lun = 0;
	req.msg.cmd = 0xD5;
	req.msg.data_len = 20;
	req.msg.data = data;

	memset(data, 0, sizeof(data));
	data[0] = 0x00; /* set */
	data[1] = 0x04; /* set drive status */
	data[2] = 0x0e; /* length lsb */
	data[3] = 0x00; /* length msb */
	data[4] = 0x00; /* offset lsb */
	data[5] = 0x00; /* offset msb */
	data[6] = 0x0e; /* length lsb */
	data[7] = 0x00; /* length msb */
	data[8] = bayId; /* bayid */
	data[9] = slotId; /* slotid */
	data[10] = state & 0xff; /* state LSB */
	data[11] = state >> 8; /* state MSB; */

	rsp = intf->sendrecv(intf, &req);
	if (!rsp) {
		lprintf(LOG_ERR, "Error issuing setled command.");
		return -1;
	} else if (rsp->ccode) {
		lprintf(LOG_ERR, "Error issuing setled command: %s",
				val2str(rsp->ccode, completion_code_vals));
		return -1;
	}
	return 0;
}
/*
 * Function Name:    ipmi_getsesmask
 *
 * Description:      This function calculates bits in SES drive update
 * Return:           Mask set with bits for SES backplane update
 */
static int
ipmi_getsesmask(int argc, char **argv)
{
	int mask = 0;
	while (current_arg < argc) {
		if (!strcmp(argv[current_arg], "present"))
			mask |= (1L << 0);
		if (!strcmp(argv[current_arg], "online"))
			mask |= (1L << 1);
		if (!strcmp(argv[current_arg], "hotspare"))
			mask |= (1L << 2);
		if (!strcmp(argv[current_arg], "identify"))
			mask |= (1L << 3);
		if (!strcmp(argv[current_arg], "rebuilding"))
			mask |= (1L << 4);
		if (!strcmp(argv[current_arg], "fault"))
			mask |= (1L << 5);
		if (!strcmp(argv[current_arg], "predict"))
			mask |= (1L << 6);
		if (!strcmp(argv[current_arg], "critical"))
			mask |= (1L << 9);
		if (!strcmp(argv[current_arg], "failed"))
			mask |= (1L << 10);
		current_arg++;
	}
	return mask;
}
/*
 * Function Name:       ipmi_delloem_setled_main
 *
 * Description:         This function processes the delloem setled command
 * Input:               intf    - ipmi interface
 *                       argc    - no of arguments
 *                       argv    - argument string array
 * Output:
 *
 * Return:              return code     0 - success
 *                         -1 - failure
 */
static int
ipmi_delloem_setled_main(struct ipmi_intf * intf, int argc, char ** argv)
{
	int b,d,f, mask;
	int bayId, slotId;
	bayId = 0xFF;
	slotId = 0xFF;
	current_arg++;
	if (argc < current_arg) {
		usage();
		return -1;
	}
	/* ipmitool delloem setled info*/
	if (argc == 1 || !strcmp(argv[current_arg], "help")) {
		ipmi_setled_usage();
		return 0;
	}
	CheckSetLEDSupport(intf);
	if (!IsSetLEDSupported()) {
		lprintf(LOG_ERR, "'setled' is not supported on this system.");
		return -1;
	} else if (sscanf(argv[current_arg], "%*x:%x:%x.%x", &b,&d,&f) == 3) {
		/* We have bus/dev/function of drive */
		current_arg++;
		ipmi_getdrivemap (intf, b, d, f, &bayId, &slotId);
	} else if (sscanf(argv[current_arg], "%x:%x.%x", &b,&d,&f) == 3) {
		/* We have bus/dev/function of drive */
		current_arg++;
	} else {
		ipmi_setled_usage();
		return -1;
	}
	/* Get mask of SES flags */
	mask = ipmi_getsesmask(argc, argv);
	/* Get drive mapping */
	if (ipmi_getdrivemap (intf, b, d, f, &bayId, &slotId)) {
		return -1;
	}
	/* Set drive LEDs */
	return ipmi_setled_state (intf, bayId, slotId, mask);
}
