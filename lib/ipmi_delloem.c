/*
 * Copyright(c) 2008, Dell Inc
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
 * CONSEQUENTIAL DAMAGES(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT(INCLUDING NEGLIGENCE OR OTHERWISE)
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
#include <ipmitool/ipmi_cc.h>

#define BIT(x)  (1 << x)
#define DELL_OEM_NETFN (uint8_t)0x30
#define GET_IDRAC_VIRTUAL_MAC (uint8_t)0xC9 /* 11g Support Macros */
#define INVALID -1
#define SHARED 0
#define SHARED_WITH_FAILOVER_LOM2 1
#define DEDICATED 2
#define SHARED_WITH_FAILOVER_ALL_LOMS 3 /* 11g Support Macros */
#define SHARED 0
#define SHARED_WITH_FAILOVER_LOM2 1
#define DEDICATED 2
#define SHARED_WITH_FAILOVER_ALL_LOMS 3 /* 12g Support Strings for nic selection */
#define INVAILD_FAILOVER_MODE -2
#define INVAILD_FAILOVER_MODE_SETTINGS -3
#define INVAILD_SHARED_MODE -4
#define INVAILD_FAILOVER_MODE_STRING "ERROR: Cannot set shared with failover lom same as current shared lom."
#define INVAILD_FAILOVER_MODE_SET "ERROR: Cannot set shared with failover loms when NIC is set to dedicated Mode."
#define INVAILD_SHARED_MODE_SET_STRING "ERROR: Cannot set shared Mode for Blades."
#define LOM_ROW 6
#define LOM_COL 10
#define NIC_ROW 4
#define NIC_COL 50
#define NIC_12G_COL 50

#ifdef SUPPORT_8_LOM
#define MAX_NUM_LOMS_IDX 11
#define RESERVED_LOM_IDX 6
#define FAILOVER_LOM_START_IDX  2
#define FAILOVER_LOM_STRING_IDX 8
#else
#define MAX_NUM_LOMS_IDX 6
#define RESERVED_LOM_IDX 6
#define FAILOVER_LOM_START_IDX  2
#define FAILOVER_LOM_STRING_IDX 3
#endif
#define SHARED_LOM1_IDX 2
#define SHARED_LOM2_IDX 3
#define SHARED_LOM3_IDX 4
#define SHARED_LOM4_IDX 5
#ifdef SUPPORT_8_LOM
#define SHARED_LOM5_IDX 7
#define SHARED_LOM6_IDX 8
#define SHARED_LOM7_IDX 9
#define SHARED_LOM8_IDX 10
#endif
#define SHARED_FAILOVER_LOM1_IDX 2
#define SHARED_FAILOVER_LOM2_IDX 3
#define SHARED_FAILOVER_LOM3_IDX 4
#define SHARED_FAILOVER_LOM4_IDX 5
#define SHARED_FAILOVER_ALL_IDX  6
#ifdef SUPPORT_8_LOM
#define SHARED_FAILOVER_LOM5_IDX 7
#define SHARED_FAILOVER_LOM6_IDX 8
#define SHARED_FAILOVER_LOM7_IDX 9
#define SHARED_FAILOVER_LOM8_IDX 10
#endif

char acivelom_string[LOM_ROW][LOM_COL] = {
	"None", 
	"LOM1", 
	"LOM2", 
	"LOM3", 
	"LOM4", 
	"dedicated"
};

/* 11g Support Strings for nic selection */
char nic_selection_mode_string[NIC_ROW][NIC_COL] = {
	"shared", 
	"shared with failover lom2", 
	"dedicated", 
	"shared with Failover all loms"
};

char nic_selection_mode_string_12g[][NIC_12G_COL] = {
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

const struct vflashstr vflash_completion_code_vals[] = {
	{0x00, "SUCCESS"}, 
	{0x01, "NO_SD_CARD"}, 
	{0x63, "UNKNOWN_ERROR"}, 
	{0x00, NULL}
};

static int current_arg = 0;
uint8_t iDRAC_FLAG = 0;

/*
 * new flags for
 * 11G || 12G || 13G  -> _ALL
 * 12G || 13G -> _12_13
 *
 */
uint8_t iDRAC_FLAG_ALL = 0;
uint8_t iDRAC_FLAG_12_13 = 0;

lcd_mode_t lcd_mode;
static uint8_t lcdsupported = 0;
static uint8_t setledsupported = 0;
int g_modularsupportlom = 0;

volatile uint8_t IMC_Type = IMC_IDRAC_10G;
struct fw_ver {
	uint8_t major_ver;
	uint8_t minor_ver;
	uint8_t build_no_msb;
	uint8_t build_no_lsb;
	uint8_t pla_data_msb;
	uint8_t pla_data_lsb;
	uint8_t lc_ver_msb;
	uint8_t lc_ver_lsb;
};

power_headroom powerheadroom;

uint8_t powercapsetable_flag = 0;
uint8_t powercapstatusflag = 0;
int boardoffsetptr;
int productoffsetptr;

static void usage(void);
/* Systeminfo Function prototypes */
static int oem_dell_sysinfo_main(struct ipmi_intf *intf, int argc, char **argv);
static char* oem_dell_get_bib_info(uint8_t *fru_data, uint8_t bib_type);
static int oem_dell_sysinfo_get_tag(struct ipmi_intf *intf, char *tagstring, uint8_t tag_type, uint8_t length);
static void oem_dell_sysinfo_print_board(struct ipmi_intf *intf, struct
					fru_info *fru, uint8_t id, uint32_t offset, uint8_t ipmi_version);
static int oem_dell_sysinfo_fru(struct ipmi_intf *intf, uint8_t id, uint8_t ipmi_version);
static int oem_dell_sysinfo_id(struct ipmi_intf *intf, int type);
static void oem_dell_sysinfo_usage(void);
/* LCD Function prototypes */
static int oem_dell_lcd_main(struct ipmi_intf *intf, int argc, char **argv);
int ipmi_lcd_get_platform_model_name(struct ipmi_intf *intf, char *lcdstring, uint8_t max_length, uint8_t field_type);
static int oem_dell_idracvalidator_command(struct ipmi_intf *intf);
static int oem_dell_lcd_get_configure_command_wh(struct ipmi_intf *intf);
static int oem_dell_lcd_get_configure_command(struct ipmi_intf *intf, uint8_t *command);
static int oem_dell_lcd_set_configure_command(struct ipmi_intf *intf, int command);
static int oem_dell_lcd_set_configure_command_wh(struct ipmi_intf *intf, uint32_t  mode, 
						 uint16_t lcdquallifier, uint8_t errordisp);
static int oem_dell_lcd_get_single_line_text(struct ipmi_intf *intf, char *lcdstring, uint8_t max_length);
static int oem_dell_lcd_get_info_wh(struct ipmi_intf *intf);
static int oem_dell_lcd_get_info(struct ipmi_intf *intf);
static int oem_dell_lcd_get_status_val(struct ipmi_intf *intf, lcd_status *lcdstatus);
static int oem_dell_is_lcd_supported(void);
static void oem_dell_check_lcd_support(struct ipmi_intf *intf);
static void oem_dell_ipmi_lcd_status_print(lcd_status lcdstatus);
static int oem_dell_lcd_get_status(struct ipmi_intf *intf);
static int oem_dell_lcd_set_kvm(struct ipmi_intf *intf, char status);
static int oem_dell_lcd_set_lock(struct ipmi_intf *intf, char lock);
static int oem_dell_lcd_set_single_line_text(struct ipmi_intf *intf, char *text);
static int oem_dell_lcd_set_text(struct ipmi_intf *intf, char *text, int line_number);
static int oem_dell_lcd_configure_wh(struct ipmi_intf *intf, uint32_t mode, 
				uint16_t lcdquallifier, uint8_t errordisp, int8_t line_number, char *text);
static int oem_dell_lcd_configure(struct ipmi_intf *intf, int command, int8_t line_number, char *text);
static void oem_dell_lcd_usage(void);
/* MAC Function prototypes */
static int oem_dell_mac_main(struct ipmi_intf *intf, int argc, char **argv);
static void oem_dell_init_embedded_nic_mac_address_values(void);
static int oem_dell_macinfo_drac_idrac_virtual_mac(struct ipmi_intf *intf, uint8_t nicnum);
static int oem_dell_macinfo_drac_idrac_mac(struct ipmi_intf *intf, uint8_t nicnum);
static int oem_dell_macinfo_10g(struct ipmi_intf *intf, uint8_t nicnum);
static int oem_dell_macinfo_11g(struct ipmi_intf *intf, uint8_t nicnum);
static int oem_dell_macinfo(struct ipmi_intf *intf, uint8_t nicnum);
static void oem_dell_mac_usage(void);
/* LAN Function prototypes */
static int oem_dell_lan_main(struct ipmi_intf *intf, int argc, char **argv);
static int oem_dell_islansupported(void);
static int oem_dell_get_nic_selection_mode(int current_arg, char **argv);
static int oem_dell_lan_set_nic_selection(struct ipmi_intf *intf, uint8_t nic_selection);
static int oem_dell_lan_get_nic_selection(struct ipmi_intf *intf);
static int oem_dell_lan_get_active_nic(struct ipmi_intf *intf);
static void oem_dell_lan_usage(void);
static int oem_dell_lan_set_nic_selection_12g(struct ipmi_intf *intf, uint8_t *nic_selection);
/* Power monitor Function prototypes */
static int oem_dell_powermonitor_main(struct ipmi_intf *intf, int argc, char **argv);
static void oem_dell_time_to_str(time_t rawTime, char *strTime);
static int oem_dell_get_sensor_reading(struct ipmi_intf *intf, unsigned char sensorNumber, sensorreadingtype *psensorreadingdata);
static int oem_dell_get_power_capstatus_command(struct ipmi_intf *intf);
static int oem_dell_set_power_capstatus_command(struct ipmi_intf *intf, uint8_t val);
static int oem_dell_powermgmt(struct ipmi_intf *intf);
static int oem_dell_powermgmt_clear(struct ipmi_intf *intf, uint8_t clearValue);
static uint64_t oem_dell_watt_to_btuphr_conversion(uint32_t powerinwatt);
static uint32_t oem_dell_btuphr_to_watt_conversion(uint64_t powerinbtuphr);
static int oem_dell_get_power_headroom_command(struct ipmi_intf *intf, uint8_t unit);
static int oem_dell_get_power_consumption_data(struct ipmi_intf *intf, uint8_t unit);
static int oem_dell_get_instan_power_consmpt_data(struct ipmi_intf *intf, 
						ipmi_inst_power_consumption_data *instpowerconsumptiondata);
static void oem_dell_print_get_instan_power_Amps_data(ipmi_inst_power_consumption_data instpowerconsumptiondata);
static int oem_dell_print_get_power_consmpt_data(struct ipmi_intf *intf, uint8_t  unit);
static int oem_dell_get_avgpower_consmpt_history(struct ipmi_intf *intf, ipmi_avgpower_consump_history *pavgpower);
static int oem_dell_get_peakpower_consmpt_history(struct ipmi_intf *intf, ipmi_power_consump_history *pstpeakpower);
static int oem_dell_get_minpower_consmpt_history(struct ipmi_intf *intf, ipmi_power_consump_history *pstminpower);
static int oem_dell_print_power_consmpt_history(struct ipmi_intf *intf, int unit);
static int oem_dell_get_power_cap(struct ipmi_intf *intf, ipmi_power_cap *ipmipowercap);
static int oem_dell_print_power_cap(struct ipmi_intf *intf, uint8_t unit);
static int oem_dell_set_power_cap(struct ipmi_intf *intf, int unit, int val);
static void oem_dell_powermonitor_usage(void);
/* vflash Function prototypes */
static int oem_dell_vflash_main(struct ipmi_intf *intf, int argc, char **argv);
const char* get_vflash_compcode_str(uint8_t vflashcompcode, const struct vflashstr *vs);
static int oem_dell_get_sd_card_info(struct ipmi_intf *intf);
static int oem_dell_vflash_process(struct ipmi_intf *intf, int current_arg, char **argv);
static void oem_dell_vflash_usage(void);
/* LED Function prototypes */
static int oem_dell_getsesmask(int, char **argv);
static void oem_dell_checksetledsupport(struct ipmi_intf *intf);
static int oem_dell_issetledsupported(void);
static void oem_dell_setled_usage(void);
static int oem_dell_setled_main(struct ipmi_intf *intf, int argc, char **argv);
static int oem_dell_setled_state(struct ipmi_intf *intf, int bayId, int slotId, int state);
static int oem_dell_getdrivemap(struct ipmi_intf *intf, int bus, int device, int function, int *bayId, int *slotId);
static int oem_dell_get_nic_selection_mode_12g(struct ipmi_intf *intf, int current_arg, char **argv, char *nic_set);

/* Function Name:       ipmi_oem_dell_main
 *
 * Description:         This function processes the delloem command
 * Input:               intf    - ipmi interface
 *                      argc    - no of arguments
 *                      argv    - argument string array
 * Output:
 *
 * Return:              return code     0 - success
 *                                     -1 - failure
 */
int
ipmi_delloem_main(struct ipmi_intf *intf, int argc, char **argv)
{
	int rc = 0;
	current_arg = 0;
	oem_dell_idracvalidator_command(intf);
	oem_dell_check_lcd_support(intf);
	if(argc == 0 || strcmp(argv[0], "help") == 0) {
		usage();
		return 0;
	}
	/* System information */
	if(strcmp(argv[current_arg], "sysinfo") == 0) {
		rc = oem_dell_sysinfo_main(intf, argc, argv);
	}
	else if(strcmp(argv[current_arg], "lcd") == 0) {
		rc = oem_dell_lcd_main(intf, argc, argv);
	} else if(strcmp(argv[current_arg], "mac") == 0) {
		/* mac address*/
		rc = oem_dell_mac_main(intf, argc, argv);
	} else if(oem_dell_islansupported() && strcmp(argv[current_arg], "lan") == 0) {
		/* lan address*/
		rc = oem_dell_lan_main(intf, argc, argv);
	} else if(strcmp(argv[current_arg], "setled") == 0) {
		/* SetLED support */
		rc = oem_dell_setled_main(intf, argc, argv);
	} else if(strcmp(argv[current_arg], "powermonitor") == 0) {
		/*Powermanagement report processing*/
		rc = oem_dell_powermonitor_main(intf, argc, argv);
	} else if(strcmp(argv[current_arg], "vflash") == 0) {
		/* vflash Support */
		rc = oem_dell_vflash_main(intf, argc, argv);
	} else{
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
static
void usage(void)
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
			"    sysinfo");
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
			"    vflash");
	lprintf(LOG_NOTICE, 
			"");
	lprintf(LOG_NOTICE, 
			"For help on individual commands type:");
	lprintf(LOG_NOTICE, 
			"delloem <command> help");
}

/*
* Function Name:       oem_dell_sysinfo_main
*
* Description:         This function processes the delloem sysinfo command
* Input:               intf    - ipmi interface
                       argc    - no of arguments
                       argv    - argument string array
* Output:
*
* Return:              return code     0 - success
*                         -1 - failure
*
*/

static
int oem_dell_sysinfo_main(struct ipmi_intf *intf, int argc, char **argv)
{
	int rc = 0;
	current_arg++;
	if(argc <= current_arg) {
		rc = oem_dell_sysinfo_id(intf, 0);
	} else{
		oem_dell_sysinfo_usage();
	}
	return rc;
}

/*
* Function Name: oem_dell_get_bib_info
*
* Description:This function retrieves following info from BIB(BIOS information block)
* section of the FRU:
*              o PDRODUCT_MODEL
*              o ASSET_TAG
*              o SERVICE_TAG
*              o BIOS_VERSION
*
* Input: fru_data - pointer to fru area
* bib_type - Product / Asset tag / Service tag / Bios version
* Output:
* Return:bib_data - BIOS information block data
*
*/
static
char* oem_dell_get_bib_info(uint8_t *fru_data, uint8_t bib_type)
{
	uint16_t bib_offset = 0, bib_info_area = 0; 
	uint16_t bib_length = 0, bib_end, length = 0;
	uint32_t offset = 0;
	char *bib_data =  NULL;

	/* get BIB fields */
	if(fru_data[FRU_OFFSET_66] == IPMI_CC_TIMEOUT) {
		bib_offset = (fru_data[FRU_OFFSET_68] << FRU_SHIFT) | fru_data[FRU_OFFSET_67];
	} else if(fru_data[FRU_OFFSET_69] == IPMI_CC_TIMEOUT) {
		bib_offset = (fru_data[FRU_OFFSET_6B] << FRU_SHIFT) | fru_data[FRU_OFFSET_6A];
	}
	bib_info_area = bib_offset + FRU_VAL;
	bib_length = (fru_data[bib_info_area+2]<<FRU_SHIFT) | fru_data[bib_info_area+1];
	bib_end = bib_info_area + bib_length;

	/* find BIB field */
	offset = bib_info_area;
	while((offset <= bib_end) && (fru_data[offset] != bib_type)) {
		offset += fru_data[offset+1]+3;
	}
	if(fru_data[offset] == bib_type) {/* bib info found */
		if(bib_type == BIB_TYPE_PDRODUCT_MODEL) {
			length = fru_data[offset+1]-FRU_VAL;
		} else{
			length = fru_data[offset+1];
		}

		bib_data = malloc(length+1);
		memset(bib_data, 0, length+1);
		if(bib_type == BIB_TYPE_PDRODUCT_MODEL) {
			memcpy(bib_data, &fru_data[offset+8], length);
		} else{
			memcpy(bib_data, &fru_data[offset+2], length);
		}
		bib_data[length] = '\0';
		/*printf("BIOS version: %s\n", bib_data);*/
	} else{
		bib_data = NULL;
	}
	return bib_data;
}

/*
* Function Name: oem_dell_sysinfo_get_tag
*
* Description:This function retrieves tag string
*
* Input: intf - pointer to interface
* tag_type - either assettag / servicetag
* length- length of the tagstring
* Output: tagstring - assettag  / servicetag string
*
* Return:0
*
*/

static
int oem_dell_sysinfo_get_tag(struct ipmi_intf *intf, 
		char *tagstring, uint8_t tag_type, uint8_t length)
{
	struct ipmi_rs *rsp = NULL;
	struct ipmi_rq req = {0};
	uint8_t data_offset = 0;
	uint8_t data[4] = {0};
	ipmi_dell_tag *tagblock = NULL;
	memset(&req, 0, sizeof(req));
	req.msg.netfn = IPMI_NETFN_APP;
	req.msg.lun = 0;
	req.msg.cmd = IPMI_GET_SYS_INFO;
	req.msg.data_len = sizeof(data);
	req.msg.data = data;
	data[data_offset++] = 0; /* get parameter*/
	data[data_offset++] = tag_type; /* get asset tag*/
	data[data_offset++] = 0;
	data[data_offset++] = 0;
	rsp = intf->sendrecv(intf, &req);
	if(NULL == rsp) {
		lprintf(LOG_ERR, 
				" Error getting asset tag");
		return -1;
	} else if( rsp->ccode ) {
		lprintf(LOG_ERR, 
				" Error getting asset tag: %s", 
				val2str(rsp->ccode, completion_code_vals));
		return -1;
	}
	tagblock = (ipmi_dell_tag *)(void *) rsp->data;
	memset(tagstring, 0, length);
	memcpy(tagstring, &tagblock->tag, tagblock->length);
	tagstring[length] = '\0' ;
	return 0;

}
/*
* Function Name: oem_dell_sysinfo_print_board
*
* Description:This function prints FRU Board Area for sysinfo command
* Input: intf - pointer to ipmi interface
* fru- pointer to fru area
* id- fru id
* offset- offset to fru area
* ipmi_version- ipmi version
* Output:
*
* Return:
*
*/
static
void oem_dell_sysinfo_print_board(struct ipmi_intf *intf, struct fru_info *fru, 
				uint8_t id, uint32_t offset, uint8_t ipmi_version)
{
	char *fru_area = NULL;
	uint8_t *fru_data = NULL, *fru_data_tmp = NULL;
	uint8_t language = 0;
	uint32_t fru_len = 0, area_len = 0, i = 0, j = 0;
	uint16_t length = 0;
	
	i = offset;
	fru_data_tmp = fru_data = malloc(fru->size + 1);
	if( NULL == fru_data ) {
		lprintf(LOG_ERR, " Out of memory!");
		return;
	}
	memset(fru_data, 0, fru->size + 1);
	if(read_fru_area(intf, fru, id, i, 2, fru_data_tmp) == 0) /* read enough to check length field */
		fru_len = 8 * fru_data[1]; //fru_len = 8 * fru_data[i + 1];
	if(fru_len <= 0) {
		free_n(&fru_data);
		return;
	}
	fru_len = 0x200;
	i = 0;
	if(read_fru_area(intf, fru, id, i, fru_len, fru_data_tmp) < 0) { /* read in the full fru */
		free_n(&fru_data);
		return;
	}
	boardoffsetptr = (fru_data[BOARD_OFFSET])*OFFSETMULTIPLIER;
	productoffsetptr = (fru_data[PRODUCT_OFFSET])*OFFSETMULTIPLIER;
	if(iDRAC_FLAG == IDRAC_14G)
		language = fru_data[boardoffsetptr+BOARDLANGOFFSET];
	else if(iDRAC_FLAG == IDRAC_13G)
		language = fru_data[10];
	if(language == 0x00) {
		printf("Board Language Code : English\n");
	} else{
		printf("Board Language Code : Unknown\n");
	}
	if(iDRAC_FLAG == IDRAC_14G)
		i = boardoffsetptr+BOARDPRODUCTMANFLENOFFSET;
	else if(iDRAC_FLAG == IDRAC_13G) {
		i = 8;
		i++; /* skip fru area version */
		area_len = fru_data[i++] * 8; /* fru area length */
		i++; /* skip fru board language */
		i += 3; /* skip mfg. date time */
	}
	fru_area = get_fru_area_str(fru_data, &i);
	if((NULL != fru_area) && (strlen(fru_area) > 0)) {
		free_n(&fru_area);
	}
	fru_area = get_fru_area_str(fru_data, &i);
	if((NULL != fru_area) && (strlen(fru_area) > 0)) {
		printf("Board Product Name  : %s\n", fru_area);
		free_n(&fru_area);
	}
	fru_area = get_fru_area_str(fru_data, &i);
	if(NULL != fru_area && strlen(fru_area) > 0) {
		printf("Board Serial Number : %s\n", fru_area);
		free_n(&fru_area);
	}
	fru_area = get_fru_area_str(fru_data, &i);
	if(NULL != fru_area && strlen(fru_area) > 0)
	{
		printf("Board Part Number   : %s\n", fru_area);
		free_n(&fru_area);
	}
	fru_area = get_fru_area_str(fru_data, &i);
	if(NULL != fru_area && strlen(fru_area) > 0)
	{
		if(*fru_area<48)
			*fru_area = '\0';
		printf("Board FRU File ID   : %s\n", fru_area);
		free_n(&fru_area);
	}
	while((fru_data[i] != IPMI_CC_INV_CMD) && (i < offset + area_len)) /* read any extra fields */
	{
		int j = i;
		fru_area = get_fru_area_str(fru_data, &i);
		if(NULL != fru_area && strlen(fru_area) > 0) {
			printf(" Board Extra           : %s\n", fru_area);
			free_n(&fru_area);
		}
		if(i == j)
			break;
	}
	if(ipmi_version == 0x51) {
		length = (fru_data[0x72] << 8) |(fru_data[0x71]); /* get host name */
		fru_area = malloc(length+1);
		memset(fru_area, 0, length+1);
		memcpy(fru_area, &fru_data[0x75], length);
		fru_area[length] = '\0';
		printf("Host Name           : %s\n", fru_area);
		free_n(&fru_area);

		fru_area = oem_dell_get_bib_info(fru_data, BIB_TYPE_PDRODUCT_MODEL);
		if(NULL != fru_area && strlen(fru_area) > 0) {
			printf("Product Model       : %s\n", fru_area);
			free_n(&fru_area);
		}
		fru_area = oem_dell_get_bib_info(fru_data, BIB_TYPE_ASSET_TAG);
		if(NULL != fru_area && strlen(fru_area) > 0) {
			printf("##Asset Tag           : %s\n", fru_area);
			free_n(&fru_area);
		}
		fru_area = oem_dell_get_bib_info(fru_data, BIB_TYPE_SERVICE_TAG);
		if(NULL != fru_area && strlen(fru_area) > 0) {
			printf("Service Tag         : %s\n", fru_area);
			free_n(&fru_area);
		}
		fru_area = oem_dell_get_bib_info(fru_data, BIB_TYPE_BIOS_VERSION);
		if(NULL != fru_area && strlen(fru_area) > 0) {
			printf("BIOS version        : %s\n", fru_area);
			free_n(&fru_area);
		}
	}
	free_n(&fru_data);
}

/*****************************************************************
 * Function Name: oem_dell_sysinfo_fru
 *
 * Description:This function prints the 'fru' portion of the sysinfo command
 * Input: intf - pointer to ipmi interface
 *id- fru id
 *ipmi_version- ipmi version
 * Output:
 *
 * Return:0
 *
 ******************************************************************/
static
int oem_dell_sysinfo_fru(struct ipmi_intf *intf, uint8_t id, uint8_t ipmi_version)
{
	struct ipmi_rs *rsp;
	struct ipmi_rq req = {0};
	struct fru_info fru = {0};
	struct fru_header header = {0};
	uint8_t msg_data[4] = {0};
	uint8_t input_length = 0;
	memset(&fru, 0, sizeof(struct fru_info));
	memset(&header, 0, sizeof(struct fru_header));
	memset(msg_data, 0, 4); /*  get info about this FRU  */
	msg_data[0] = id;
	memset(&req, 0, sizeof(req));
	req.msg.netfn = IPMI_NETFN_STORAGE;
	req.msg.lun = 0;
	req.msg.cmd = GET_FRU_INFO;
	req.msg.data = msg_data;
	req.msg.data_len = 1;
	rsp = intf->sendrecv(intf, &req);
	if(NULL == rsp) {
		printf(" Device not present(No Response)\n");
		return -1;
	}
	if( rsp->ccode ) {
		printf(" Device not present(%s)\n", 
				val2str(rsp->ccode, completion_code_vals));
		return -1;
	}
	fru.size = (rsp->data[1] << 8) | rsp->data[0];
	fru.access = rsp->data[2] & 0x1;
	lprintf(LOG_DEBUG, 
			"fru.size = %d bytes(accessed by %s)", 
			fru.size, fru.access ? "words" : "bytes");
	if(fru.size < 1) {
		lprintf(LOG_ERR, 
				" Invalid FRU size %d", fru.size);
		return -1;
	}

	/*
	 * retrieve the FRU header
	 */
	msg_data[input_length++] = id;
	msg_data[input_length++] = 0;
	msg_data[input_length++] = 0;
	msg_data[input_length++] = 8;

	memset(&req, 0, sizeof(req));
	req.msg.netfn = IPMI_NETFN_STORAGE;
	req.msg.lun = 0;
	req.msg.cmd = GET_FRU_DATA;
	req.msg.data = msg_data;
	req.msg.data_len = REQ_MSG_LEN;

	rsp = intf->sendrecv(intf, &req);
	if(NULL == rsp) {
		printf(" Device not present(No Response)\n");
		return 1;
	}
	if( rsp->ccode ) {
		printf(" Device not present(%s)\n", 
				val2str(rsp->ccode, completion_code_vals));
		return 1;
	}

	if(verbose > 1)
		printbuf(rsp->data, rsp->data_len, "FRU DATA");

	memcpy(&header, rsp->data + 1, 8);

	if(header.version != 1)
	{
		lprintf(LOG_ERR, 
				" Unknown FRU header version 0x%02x", 
				header.version);
		return -1;
	}

	/* offsets need converted to bytes
	 * but that conversion is not done to the structure
	 * because we may end up with offset > 255
	 * which would overflow our 1-byte offset field */


#define IPMI_DELL_OS_NAME_LENGTH_MAX 254

	char assettag[IPMI_DELL_SYSINFO_ASSET_TAG_LENGTH+1] = {0};
	char servicetag[IPMI_DELL_SYSINFO_SERVICE_TAG_LENGTH+1] = {0};
	char hostname[IPMI_DELL_LCD_STRING_LENGTH_MAX+1] = {0};
	char productmodel[IPMI_DELL_LCD_STRING_LENGTH_MAX+1] = {0};
	char osname[IPMI_DELL_OS_NAME_LENGTH_MAX+1] = {0};
	char biosversion[IPMI_DELL_LCD_STRING_LENGTH_MAX+1] = {0};
	char osversion[IPMI_DELL_LCD_STRING_LENGTH_MAX+1] = {0};

	/* Print board area */
	if((header.offset.board*8) >= sizeof(struct fru_header))
		oem_dell_sysinfo_print_board(intf, &fru, id, header.offset.board*8, ipmi_version);

	/* Print the rest of the sysinfo command */
	if(ipmi_version == 0x51) {

		/*all the remaining info for ipmi 1.5 systems is printed in the oem_dell_sysinfo_print_board
		  function call above. */
	} else{
		/* for IPMI 2.0 we get the remaining info from the "get system info" IPMI command rather
		 * than from the 'get fru data' command */


		ipmi_lcd_get_platform_model_name(intf, hostname, IPMI_DELL_LCD_STRING_LENGTH_MAX, 
				IPMI_DELL_SYSINFO_HOST_NAME);
		printf("Host Name           : %s\n", hostname);


		ipmi_lcd_get_platform_model_name(intf, productmodel, IPMI_DELL_LCD_STRING_LENGTH_MAX, 
				IPMI_DELL_PLATFORM_MODEL_NAME_SELECTOR);
		printf("Product Model       : %s\n", productmodel);

		oem_dell_sysinfo_get_tag(intf, assettag, IPMI_DELL_SYSINFO_ASSET_TAG, 
				IPMI_DELL_SYSINFO_ASSET_TAG_LENGTH);
		printf("Asset Tag           : %s\n", assettag);

		oem_dell_sysinfo_get_tag(intf, servicetag, IPMI_DELL_SYSINFO_SERVICE_TAG, 
				IPMI_DELL_SYSINFO_SERVICE_TAG_LENGTH);
		printf("Service Tag         : %s\n", servicetag);

		ipmi_lcd_get_platform_model_name(intf, biosversion, IPMI_DELL_LCD_STRING_LENGTH_MAX, 
				IPMI_DELL_SYSINFO_BIOS_VERSION);
		printf("BIOS Version        : %s\n", biosversion);

		ipmi_lcd_get_platform_model_name(intf, osname, IPMI_DELL_OS_NAME_LENGTH_MAX, 
				IPMI_DELL_SYSINFO_OS_NAME);
		printf("System OS Name      : %s\n", osname);

		ipmi_lcd_get_platform_model_name(intf, osversion, IPMI_DELL_OS_NAME_LENGTH_MAX, 
				IPMI_DELL_SYSINFO_OS_VERSION_NUMBER);
		printf("System OS Version   : %s\n", osversion);
	}
	return 0;
}

/*****************************************************************
 * Function Name: oem_dell_sysinfo_id
 *
 * Description:This function prints the 'id' portion of the sysinfo command
 * Input: intf - pointer to ipmi interface
 *type- fru type
 * Output:
 *
 * Return:
 *
 ******************************************************************/
static
int oem_dell_sysinfo_id(struct ipmi_intf *intf, int type)
{
	struct ipmi_rs *rsp = NULL;
	struct ipmi_rq req = {0};
	int rc = 0;
	struct ipm_devid_rsp *dev_id = NULL;
	struct sdr_repo_info_rs *repo_info = NULL;
	uint8_t ipmi_version = 0;
	uint8_t data[4] = {0};
	uint8_t input_length = 0;
	struct fw_ver firmware_ver = {0};

	/* 20 80 01 52 51 9f a2 02 00 00 00 00 00 00 00 <- raw data */
	memset(&req, 0, sizeof(req));

	if((iDRAC_FLAG == IDRAC_13G) ||(iDRAC_FLAG == IDRAC_14G))
		data[input_length] = IPMI_IDRAC_VER2;
	else if(iDRAC_FLAG == IDRAC_12G)
		data[input_length] = IPMI_IDRAC_VER1;
	req.msg.netfn = DELL_OEM_NETFN;
	req.msg.lun = 0;
	req.msg.cmd = SYSINFO_ID_CMD;
	req.msg.data = data;
	req.msg.data_len = 1;
	rsp = intf->sendrecv(intf, &req);
	if(NULL == rsp)
	{
		printf(" Error Getting the Firmware version \n");
		return -1;
	}
	if(rsp->ccode == IPMI_CC_INV_CMD)
	{
		memset(&firmware_ver, 0, sizeof(struct fw_ver));
	}
	else if(rsp->ccode)
	{
		printf(" Error getting device ID(%s)\n", 
				val2str(rsp->ccode, completion_code_vals));
		return -1;
	}
	else
		memcpy(&firmware_ver, rsp->data, sizeof(struct fw_ver));

	memset(&req, 0, sizeof(req));
	req.msg.netfn = IPMI_NETFN_APP;
	req.msg.lun = 0;
	req.msg.cmd = BMC_GET_DEVICE_ID;
	req.msg.data = NULL;
	req.msg.data_len = 0;

	rsp = intf->sendrecv(intf, &req);
	if(NULL == rsp)
	{
		printf(" Error getting device ID(No Response)\n");
		return -1;
	}
	if(rsp->ccode)
	{
		printf(" Error getting device ID(%s)\n", 
				val2str(rsp->ccode, completion_code_vals));
		return -1;
	}
	dev_id = (struct ipm_devid_rsp *)(void *) rsp->data;
	printf("Device ID           : %0.2d\n", dev_id->device_id);
	printf("Device Revision     : %0.2d\n", dev_id->device_revision & 0x7F);
	if((iDRAC_FLAG == IDRAC_12G) || (iDRAC_FLAG == IDRAC_13G)
			||(iDRAC_FLAG == IDRAC_14G))
	{
		if(firmware_ver.major_ver == 0)
		{
			printf("Firmware Version    : %d.%0.2d(Build %02d)\n", 
					dev_id->fw_rev1 & 0x7F, dev_id->fw_rev2, 
					dev_id->aux_fw_rev[1]);
		}
		else
		{
			if((iDRAC_FLAG == IDRAC_13G) || (iDRAC_FLAG == IDRAC_14G))
			{
				printf
					("Firmware Version    : %d.%0.2d.%02d.%02d(Build %02d)\n", 
					 firmware_ver.major_ver, firmware_ver.minor_ver, 
					(firmware_ver.pla_data_msb << 8 | firmware_ver.pla_data_lsb), 
					(firmware_ver.lc_ver_msb << 8 | firmware_ver.lc_ver_lsb), 
					(firmware_ver.build_no_msb << 8 | firmware_ver.
					  build_no_lsb));
			}
			else
			{
				printf("Firmware Version    : %d.%0.2d.%02d(Build %02d)\n", 
						firmware_ver.major_ver, firmware_ver.minor_ver, 
						(firmware_ver.pla_data_msb << 8 | firmware_ver.
						 pla_data_lsb), 
						(firmware_ver.build_no_msb << 8 | firmware_ver.
						 build_no_lsb));
			}
		}
	}
	else
	{
		/*DF427387 Fix for Build Version Number
		*Byte:  1314   15       16
		*Data: <platform ID> <Build Number> <RES> <RES>
		*/
		printf("Firmware Version    : %d.%0.2x.%02d\n", dev_id->fw_rev1 & 0x7F, 
				dev_id->fw_rev2, dev_id->aux_fw_rev[1]);
	}

	/* BCD value. byte->hex conversion used. */
	printf("IPMI Version        : %d.%d\n", dev_id->ipmi_version & 0x0F, 
			dev_id->ipmi_version >> 4);
	ipmi_version = dev_id->ipmi_version;

	printf("Manufacturer ID     : %lu\n", 
			(long)IPM_DEV_MANUFACTURER_ID(dev_id->manufacturer_id));
	printf("Product ID          : %d\n", 
			dev_id->product_id[1] * 256 + dev_id->product_id[0]);

	memset(&req, 0, sizeof(req));
	req.msg.netfn = IPMI_NETFN_APP;
	req.msg.lun = 0;
	req.msg.cmd = BMC_GET_SELF_TEST;
	req.msg.data = NULL;
	req.msg.data_len = 0;
	rsp = intf->sendrecv(intf, &req);

	if(rsp->data[0] == 0x55)
	{
		printf("Status              : No error\n");
	}
	else if(rsp->data[0] == 0x56)
	{
		printf
			("Status              : Self Test Function not implemented in this controller\n");
	}
	else if(rsp->data[0] == 0x58)
	{
		printf
			("Status              : Fatal Hardware Error(BMC is inoperative)\n");
	}
	else if(rsp->data[0] == 0x57)
	{
		printf("Status              : ");
		if(rsp->data[1] & IPMI_VAL1)
		{
			printf("Controller operational firmware corrupte ");
		}
		if(rsp->data[1] & IPMI_VAL2)
		{
			printf("Controller update 'boot block' firmware corrupte ");
		}
		if(rsp->data[1] & IPMI_VAL3)
		{
			printf("Internal Use Area of BMC FRU corrupted ");
		}
		if(rsp->data[1] & IPMI_VAL4)
		{
			printf("SDR Repository Empty ");
		}
		if(rsp->data[1] & IPMI_VAL5)
		{
			printf("IPMB signal lines do not respond ");
		}
		if(rsp->data[1] & IPMI_VAL6)
		{
			printf("Cannot access BMC FRU device ");
		}
		if(rsp->data[1] & IPMI_VAL7)
		{
			printf("Cannot access SDR Repository ");
		}
		if(rsp->data[1] & IPMI_VAL8)
		{
			printf("Cannot access SEL device");
		}
		printf("\n");
	}
	else if(rsp->data[0] != 0xFF)
	{
		printf
			("Status              : Internal Failure. Refer to particular device's specification for definition\n");
	}
	else
	{
		printf("Status              : \n");
	}
	memset(&req, 0, sizeof(req));
	req.msg.netfn = IPMI_NETFN_STORAGE;
	req.msg.lun = 0;
	req.msg.cmd = GET_SDR_REPO_INFO;
	req.msg.data = NULL;
	req.msg.data_len = 0;
	rsp = intf->sendrecv(intf, &req);
	if(NULL == rsp)
	{
		printf(" Error getting SDR version(No Response)\n");
		return -1;
	}
	if(rsp->ccode)
	{
		printf(" Error getting SDR version(%s)\n", 
				val2str(rsp->ccode, completion_code_vals));
		return -1;
	}
	repo_info = (struct sdr_repo_info_rs *)(void *) rsp->data;
	printf("SDR Version         : %d.%d\n", repo_info->version & 0x0F, 
			repo_info->version >> 4);
	memset(&req, 0, sizeof(req));
	req.msg.netfn = IPMI_NETFN_APP;
	req.msg.lun = 0;
	req.msg.cmd = BMC_GET_GUID;
	req.msg.data = NULL;
	req.msg.data_len = 0;
	rsp = intf->sendrecv(intf, &req);
	if(NULL == rsp)
	{
		printf(" Error getting device GUID(No Response)\n");
		return -1;
	}
	if(rsp->ccode)
	{
		printf(" Error getting device GUID(%s)\n", 
				val2str(rsp->ccode, completion_code_vals));
		return -1;
	}
	printf
		("GUID                : %.2X%.2X%.2X%.2X-%.2X%.2X-%.2X%.2X-%.2X%.2X-%.2X%.2X%.2X%.2X%.2X%.2X\n", 
		 rsp->data[15], rsp->data[14], rsp->data[13], rsp->data[12], 
		 rsp->data[11], rsp->data[10], rsp->data[9], rsp->data[8], rsp->data[7], 
		 rsp->data[6], rsp->data[5], rsp->data[4], rsp->data[3], rsp->data[2], 
		 rsp->data[1], rsp->data[0]);
	rc = oem_dell_sysinfo_fru(intf, 0, ipmi_version);
	return rc;
}
/*****************************************************************
* Function Name: oem_dell_sysinfo_usage
*
* Description: This function prints help message for sysinfo command
* Input:
* Output:
*
* Return:
*
******************************************************************/

static
void oem_dell_sysinfo_usage(void)
{
	lprintf(LOG_NOTICE, 
			"");
	lprintf(LOG_NOTICE, 
			"   sysinfo");
	lprintf(LOG_NOTICE, 
			"      Show system information");
}
/*
 * Function Name:       oem_dell_lcd_main
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
static
int oem_dell_lcd_main(struct ipmi_intf *intf, int argc, char **argv)
{
	int rc = 0;
	current_arg++;
	if(argc < current_arg) {
		usage();
		return -1;
	}
	/* ipmitool delloem lcd info*/
	if(argc == 1 || strcmp(argv[current_arg], "help") == 0) {
		oem_dell_lcd_usage();
		return 0;
	}
	oem_dell_check_lcd_support(intf);
	oem_dell_idracvalidator_command(intf);
	if(!oem_dell_is_lcd_supported()) {
		lprintf(LOG_ERR, 
				"lcd is not supported on this system.");
		return -1;
	} else if(strcmp(argv[current_arg], "info") == 0) {
		if(iDRAC_FLAG_ALL) {
			rc = oem_dell_lcd_get_info_wh(intf);
		} else{
			rc = oem_dell_lcd_get_info(intf);
		}
	} else if(strcmp(argv[current_arg], "status") == 0) {
		rc = oem_dell_lcd_get_status(intf);
	} else if(strcmp(argv[current_arg], "set") == 0) {
		/* ipmitool delloem lcd set*/
		uint8_t line_number = 0;
		current_arg++;
		if(argc <= current_arg) {
			oem_dell_lcd_usage();
			return -1;
		}
		if(strcmp(argv[current_arg], "line") == 0) {
			current_arg++;
			if(argc <= current_arg) {
				usage();
				return -1;
			}
			if(str2uchar(argv[current_arg], &line_number) != 0) {
				lprintf(LOG_ERR, 
						"Argument '%s' is either not a number or out of range.", 
						argv[current_arg]);
				return(-1);
			}
			current_arg++;
			if(argc <= current_arg) {
				usage();
				return -1;
			}
		}
		if((strcmp(argv[current_arg], "mode") == 0)
				&&(iDRAC_FLAG_ALL)) {
			current_arg++;
			if(argc <= current_arg) {
				oem_dell_lcd_usage();
				return -1;
			}
			if(NULL == argv[current_arg] ) {
				oem_dell_lcd_usage();
				return -1;
			}
			if(strcmp(argv[current_arg], "none") == 0) {
				rc = oem_dell_lcd_configure_wh(intf, IPMI_DELL_LCD_CONFIG_NONE, 0xFF, 
						0XFF, 0, NULL);
			} else if(strcmp(argv[current_arg], "modelname") == 0) {
				rc = oem_dell_lcd_configure_wh(intf, IPMI_DELL_LCD_CONFIG_DEFAULT, 0xFF, 
						0XFF, 0, NULL);
			} else if(strcmp(argv[current_arg], "userdefined") == 0) {
				current_arg++;
				if(argc <= current_arg) {
					oem_dell_lcd_usage();
					return -1;
				}
				rc = oem_dell_lcd_configure_wh(intf, IPMI_DELL_LCD_CONFIG_USER_DEFINED, 
						0xFF, 0XFF, line_number, argv[current_arg]);
			} else if(strcmp(argv[current_arg], "ipv4address") == 0) {
				rc = oem_dell_lcd_configure_wh(intf, IPMI_DELL_LCD_iDRAC_IPV4ADRESS, 
						0xFF, 0XFF, 0, NULL);
			} else if(strcmp(argv[current_arg], "macaddress") == 0) {
				rc = oem_dell_lcd_configure_wh(intf, IPMI_DELL_LCD_IDRAC_MAC_ADDRESS, 
						0xFF, 0XFF, 0, NULL);
			} else if(strcmp(argv[current_arg], "systemname") == 0) {
				rc = oem_dell_lcd_configure_wh(intf, IPMI_DELL_LCD_OS_SYSTEM_NAME, 0xFF, 
						0XFF, 0, NULL);
			} else if(strcmp(argv[current_arg], "servicetag") == 0) {
				rc = oem_dell_lcd_configure_wh(intf, IPMI_DELL_LCD_SERVICE_TAG, 0xFF, 
						0XFF, 0, NULL);
			} else if(strcmp(argv[current_arg], "ipv6address") == 0) {
				rc = oem_dell_lcd_configure_wh(intf, IPMI_DELL_LCD_iDRAC_IPV6ADRESS, 
						0xFF, 0XFF, 0, NULL);
			} else if(strcmp(argv[current_arg], "ambienttemp") == 0) {
				rc = oem_dell_lcd_configure_wh(intf, IPMI_DELL_LCD_AMBEINT_TEMP, 0xFF, 
						0XFF, 0, NULL);
			} else if(strcmp(argv[current_arg], "systemwatt") == 0) {
				rc = oem_dell_lcd_configure_wh(intf, IPMI_DELL_LCD_SYSTEM_WATTS, 0xFF, 
						0XFF, 0, NULL);
			} else if(strcmp(argv[current_arg], "assettag") == 0) {
				rc = oem_dell_lcd_configure_wh(intf, IPMI_DELL_LCD_ASSET_TAG, 0xFF, 
						0XFF, 0, NULL);
			} else if(strcmp(argv[current_arg], "help") == 0) {
				oem_dell_lcd_usage();
			} else{
				lprintf(LOG_ERR, 
						"Invalid DellOEM command: %s", 
						argv[current_arg]);
				oem_dell_lcd_usage();
			}
		} else if((strcmp(argv[current_arg], "lcdqualifier") == 0)
				&&(iDRAC_FLAG_ALL)) {
			current_arg++;
			if(argc <= current_arg) {
				oem_dell_lcd_usage();
				return -1;
			}
			if( NULL == argv[current_arg] ) {
				oem_dell_lcd_usage();
				return -1;
			}
			if(strcmp(argv[current_arg], "watt") == 0) {
				rc = oem_dell_lcd_configure_wh(intf, 0xFF, 0x00, 0XFF, 0, NULL);
			} else if(strcmp(argv[current_arg], "btuphr") == 0) {
				rc = oem_dell_lcd_configure_wh(intf, 0xFF, 0x01, 0XFF, 0, NULL);
			} else if(strcmp(argv[current_arg], "celsius") == 0) {
				rc = oem_dell_lcd_configure_wh(intf, 0xFF, 0x02, 0xFF, 0, NULL);
			} else if(strcmp(argv[current_arg], "fahrenheit") == 0) {
				rc = oem_dell_lcd_configure_wh(intf, 0xFF, 0x03, 0xFF, 0, NULL);
			} else if(strcmp(argv[current_arg], "help") == 0) {
				oem_dell_lcd_usage();
			} else{
				lprintf(LOG_ERR, 
						"Invalid DellOEM command: %s", 
						argv[current_arg]);
				oem_dell_lcd_usage();
			}
		} else if((strcmp(argv[current_arg], "errordisplay") == 0)
				&&(iDRAC_FLAG_ALL)) {
			current_arg++;
			if(argc <= current_arg) {
				oem_dell_lcd_usage();
				return -1;
			}
			if( NULL == argv[current_arg] ) {
				oem_dell_lcd_usage();
				return -1;
			}
			if(strcmp(argv[current_arg], "sel") == 0) {
				rc = oem_dell_lcd_configure_wh(intf, 0xFF, 0xFF, 
						IPMI_DELL_LCD_ERROR_DISP_SEL, 0, NULL);
			} else if(strcmp(argv[current_arg], "simple") == 0) {
				rc = oem_dell_lcd_configure_wh(intf, 0xFF, 0xFF, 
						IPMI_DELL_LCD_ERROR_DISP_VERBOSE, 0, NULL);
			} else if(strcmp(argv[current_arg], "help") == 0) {
				oem_dell_lcd_usage();
			} else{
				lprintf(LOG_ERR, 
						"Invalid DellOEM command: %s", 
						argv[current_arg]);
				oem_dell_lcd_usage();
			}
		} else if((strcmp(argv[current_arg], "none") == 0)
				&&(iDRAC_FLAG == 0)) {
			rc = oem_dell_lcd_configure(intf, IPMI_DELL_LCD_CONFIG_NONE, 0, NULL);
		} else if((strcmp(argv[current_arg], "default") == 0)
				&&(iDRAC_FLAG == 0)) {
			rc = oem_dell_lcd_configure(intf, IPMI_DELL_LCD_CONFIG_DEFAULT, 0, NULL);
		} else if((strcmp(argv[current_arg], "custom") == 0)
				&&(iDRAC_FLAG == 0)) {
			current_arg++;
			if(argc <= current_arg) {
				oem_dell_lcd_usage();
				return -1;
			}
			rc = oem_dell_lcd_configure(intf, IPMI_DELL_LCD_CONFIG_USER_DEFINED, 
					line_number, argv[current_arg]);
		} else if(strcmp(argv[current_arg], "vkvm") == 0) {
			current_arg++;
			if(argc <= current_arg) {
				oem_dell_lcd_usage();
				return -1;
			}
			if(strcmp(argv[current_arg], "active") == 0) {
				rc = oem_dell_lcd_set_kvm(intf, 1);
			} else if(strcmp(argv[current_arg], "inactive") == 0) {
				rc = oem_dell_lcd_set_kvm(intf, 0);
			} else if(strcmp(argv[current_arg], "help") == 0) {
				oem_dell_lcd_usage();
			} else{
				lprintf(LOG_ERR, 
						"Invalid DellOEM command: %s", 
						argv[current_arg]);
				oem_dell_lcd_usage();
			}
		} else if(strcmp(argv[current_arg], "frontpanelaccess") == 0) {
			current_arg++;
			if(argc <= current_arg) {
				oem_dell_lcd_usage();
				return -1;
			}
			if(strcmp(argv[current_arg], "viewandmodify") == 0) {
				rc = oem_dell_lcd_set_lock(intf, 0);
			} else if(strcmp(argv[current_arg], "viewonly") == 0) {
				rc =  oem_dell_lcd_set_lock(intf, 1);
			} else if(strcmp(argv[current_arg], "disabled") == 0) {
				rc =  oem_dell_lcd_set_lock(intf, 2);
			} else if(strcmp(argv[current_arg], "help") == 0) {
				oem_dell_lcd_usage();
			} else{
				lprintf(LOG_ERR, 
						"Invalid DellOEM command: %s", 
						argv[current_arg]);
				oem_dell_lcd_usage();
			}
		} else if((strcmp(argv[current_arg], "help") == 0)
				&&(iDRAC_FLAG == 0)) {
			oem_dell_lcd_usage();
		} else{
			lprintf(LOG_ERR, 
					"Invalid DellOEM command: %s", 
					argv[current_arg]);
			oem_dell_lcd_usage();
			return -1;
		}
	} else{
		lprintf(LOG_ERR, 
				"Invalid DellOEM command: %s", 
				argv[current_arg]);
		oem_dell_lcd_usage();
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
ipmi_lcd_get_platform_model_name(struct ipmi_intf *intf, char *lcdstring, 
					uint8_t max_length, uint8_t field_type)
{
	int bytes_copied = 0;
	int i = 0;
	int lcdstring_len = 0;
	int rc = 0;
	ipmi_dell_lcd_string lcdstringblock = {0};

	for(i = 0; i < 4; i++) {
		int bytes_to_copy;
		rc = ipmi_mc_getsysinfo(intf, field_type, i, 0, sizeof(lcdstringblock), 
				&lcdstringblock);
		if(rc < 0) {
			lprintf(LOG_ERR, 
					"Error getting platform model name");
			break;
		} else if(rc > 0) {
			lprintf(LOG_ERR, 
					"Error getting platform model name: %s", 
					val2str(rc, completion_code_vals));
			break;
		}
		/* first block is different - 14 bytes*/
		if(i == 0) {
			lcdstring_len = lcdstringblock.lcd_string.selector_0_string.length;
			lcdstring_len = MIN(lcdstring_len, max_length);
			bytes_to_copy = MIN(lcdstring_len, IPMI_DELL_LCD_STRING1_SIZE);
			memcpy(lcdstring, lcdstringblock.lcd_string.selector_0_string.data, 
					bytes_to_copy);
		} else{
			int string_offset;
			bytes_to_copy = MIN(lcdstring_len - bytes_copied, 
					IPMI_DELL_LCD_STRINGN_SIZE);
			if(bytes_to_copy < 1) {
				break;
			}
			string_offset = IPMI_DELL_LCD_STRING1_SIZE + IPMI_DELL_LCD_STRINGN_SIZE
				*(i - 1);
			memcpy(lcdstring + string_offset, 
					lcdstringblock.lcd_string.selector_n_data, bytes_to_copy);
		}
		bytes_copied += bytes_to_copy;
		if(bytes_copied >= lcdstring_len) {
			break;
		}
	}
	return rc;
}
/*
 * Function Name:    oem_dell_idracvalidator_command
 *
 * Description:      This function returns the iDRAC6 type
 * Input:            intf            - ipmi interface
 * Output:
 *
 * Return:           iDRAC6 type     1 - whoville
 *                                   0 - others
 */
static
int oem_dell_idracvalidator_command(struct ipmi_intf *intf)
{
	int rc = 0;
	uint8_t data[11] = {0};
	rc = ipmi_mc_getsysinfo(intf, IPMI_DELL_IDRAC_VALIDATOR, 2, 0, sizeof(data), 
			data);
	if(rc < 0) {
		/*lprintf(LOG_ERR, " Error getting IMC type"); */
		return -1;
	} else if(rc > 0) {
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
	if((IMC_IDRAC_11G_MONOLITHIC == data[10])
			||(IMC_IDRAC_11G_MODULAR == data[10])
			||(IMC_MASER_LITE_BMC == data[10])
			||(IMC_MASER_LITE_NU == data[10])) {
		iDRAC_FLAG = IDRAC_11G;
		iDRAC_FLAG_ALL = 1;
	} else if((IMC_IDRAC_12G_MONOLITHIC == data[10])
			||(IMC_IDRAC_12G_MODULAR == data[10])) {
		iDRAC_FLAG = IDRAC_12G;
		iDRAC_FLAG_ALL = 1;
		iDRAC_FLAG_12_13 = 1;
	} else if((IMC_IDRAC_13G_MONOLITHIC == data[10])
			||(IMC_IDRAC_13G_MODULAR == data[10])
			||(IMC_IDRAC_13G_DCS == data[10])) {
		iDRAC_FLAG = IDRAC_13G;
		iDRAC_FLAG_ALL = 1;
		iDRAC_FLAG_12_13 = 1;
	} else if((IMC_IDRAC_14G_MONOLITHIC == data[10])
			||(IMC_IDRAC_14G_MODULAR == data[10])
			||(IMC_IDRAC_14G_DCS == data[10])) {
		iDRAC_FLAG = IDRAC_14G;
		iDRAC_FLAG_ALL = 1;
		iDRAC_FLAG_12_13 = 1;
	} else{
		iDRAC_FLAG = 0;
		iDRAC_FLAG_ALL = 0;
		iDRAC_FLAG_12_13 = 0;
	}
	IMC_Type = data[10];
	return 0;
}
/*
 * Function Name:    oem_dell_lcd_get_configure_command_wh
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
static
int oem_dell_lcd_get_configure_command_wh(struct ipmi_intf *intf)
{
	int rc = 0;
	rc = ipmi_mc_getsysinfo(intf, IPMI_DELL_LCD_CONFIG_SELECTOR, 0, 0, 
			sizeof(lcd_mode), &lcd_mode);
	if(rc < 0) {
		lprintf(LOG_ERR, 
				"Error getting LCD configuration");
		return -1;
	} else if((rc == IPMI_CC_INV_CMD) ||(rc == IPMI_CC_REQ_DATA_NOT_PRESENT)){
		lprintf(LOG_ERR, 
				"Error getting LCD configuration: "
				"Command not supported on this system.");
	} else if(rc > 0) {
		lprintf(LOG_ERR, 
				"Error getting LCD configuration: %s", 
				val2str(rc, completion_code_vals));
		return -1;
	}
	return 0;
}
/*
 * Function Name:    oem_dell_lcd_get_configure_command
 *
 * Description:   This function returns current lcd configuration for Dell OEM
 * LCD command
 * Input:         intf            - ipmi interface
 * Output:        command         - user defined / default / none / ipv4 / mac address /
 *                 system name / service tag / ipv6 / temp / system watt / asset tag
 *
 * Return:
 */
static
int oem_dell_lcd_get_configure_command(struct ipmi_intf *intf, uint8_t *command)
{
	uint8_t data[4] = {0};
	int rc = 0;
	rc = ipmi_mc_getsysinfo(intf, IPMI_DELL_LCD_CONFIG_SELECTOR, 0, 0, 
			sizeof(data), data);
	if(rc < 0) {
		lprintf(LOG_ERR, 
				"Error getting LCD configuration");
		return -1;
	} else if((rc == IPMI_CC_INV_CMD)||(rc == IPMI_CC_REQ_DATA_NOT_PRESENT)) {
		lprintf(LOG_ERR, 
				"Error getting LCD configuration: "
				"Command not supported on this system.");
		return -1;
	} else if(rc > 0) {
		lprintf(LOG_ERR, 
				"Error getting LCD configuration: %s", 
				val2str(rc, completion_code_vals));
		return -1;
	}
	*command = data[1]; /* rsp->data[0] is the rev */
	return 0;
}
/*
 * Function Name:    oem_dell_lcd_set_configure_command
 *
 * Description:      This function updates current lcd configuration
 * Input:            intf            - ipmi interface
 *                   command         - user defined / default / none / ipv4 / mac address /
 *                        system name / service tag / ipv6 / temp / system watt / asset tag
 * Output:
 * Return:
 */
static
int oem_dell_lcd_set_configure_command(struct ipmi_intf *intf, int command)
{
	uint8_t data[2] = {0};
	uint8_t input_length = 0;
	int rc;
	data[input_length++] = IPMI_DELL_LCD_CONFIG_SELECTOR;
	data[input_length++] = command;                      /* command - custom, default, none */
	rc = ipmi_mc_setsysinfo(intf, 2, data);
	if(rc < 0) {
		lprintf(LOG_ERR, 
				"Error setting LCD configuration");
		return -1;
	} else if((rc == IPMI_CC_INV_CMD) || (rc == IPMI_CC_REQ_DATA_NOT_PRESENT)) {
		lprintf(LOG_ERR, 
				"Error setting LCD configuration: "
				"Command not supported on this system.");
	} else if(rc) {
		lprintf(LOG_ERR, 
				"Error setting LCD configuration: %s", 
				val2str(rc, completion_code_vals));
		return -1;
	}
	return 0;
}
/*
 * Function Name:    oem_dell_lcd_set_configure_command
 *
 * Description:      This function updates current lcd configuration
 * Input:            intf            - ipmi interface
 *                   mode            - user defined / default / none
 *                   lcdquallifier   - lcd quallifier id
 *                   errordisp       - error number
 * Output:
 * Return:
 */
static
int oem_dell_lcd_set_configure_command_wh(struct ipmi_intf *intf, uint32_t  mode, 
				uint16_t lcdquallifier, uint8_t errordisp)
{
	uint8_t data[13] = {0};
	uint8_t input_length = 0;
	int rc = 0;
	oem_dell_lcd_get_configure_command_wh(intf);
	data[input_length++] = IPMI_DELL_LCD_CONFIG_SELECTOR;
	if(mode != 0xFF) {
		data[input_length++] = mode & 0xFF; /* command - custom, default, none*/
		data[input_length++] = (mode & 0xFF00) >> 8;
		data[input_length++] = (mode & 0xFF0000) >> 16;
		data[input_length++] = (mode & 0xFF000000) >> 24;
	} else{
		data[input_length++] = (lcd_mode.lcdmode) & 0xFF; /* command - custom, default, none*/
		data[input_length++] = ((lcd_mode.lcdmode) & 0xFF00) >> 8;
		data[input_length++] = ((lcd_mode.lcdmode) & 0xFF0000) >> 16;
		data[input_length++] = ((lcd_mode.lcdmode) & 0xFF000000) >> 24;
	}
	if(lcdquallifier != 0xFF) {
		if(lcdquallifier == 0x01) {
			data[input_length++] = (lcd_mode.lcdquallifier) | 0x01; /* command - custom, default, none*/
		} else  if(lcdquallifier == 0x00) {
			data[input_length++] = (lcd_mode.lcdquallifier) & 0xFE; /* command - custom, default, none*/
		} else if(lcdquallifier == 0x03) {
			data[input_length++] = (lcd_mode.lcdquallifier) | 0x02; /* command - custom, default, none*/
		} else if(lcdquallifier == 0x02) {
			data[input_length++] = (lcd_mode.lcdquallifier) & 0xFD;
		}
	} else{
		data[input_length++] = lcd_mode.lcdquallifier;
	}
	if(errordisp != 0xFF) {
		input_length += 6;
		data[input_length] = errordisp;
	} else{
		input_length += 6;
		data[input_length] = lcd_mode.error_display;
	}
	rc = ipmi_mc_setsysinfo(intf, 13, data);
	if(rc < 0) {
		lprintf(LOG_ERR, 
				"Error setting LCD configuration");
		return -1;
	} else if((rc == IPMI_CC_INV_CMD) || (rc == IPMI_CC_REQ_DATA_NOT_PRESENT)) {
		lprintf(LOG_ERR, 
				"Error setting LCD configuration: "
				"Command not supported on this system.");
	} else if( rc ) {
		lprintf(LOG_ERR, 
				"Error setting LCD configuration: %s", 
				val2str(rc, completion_code_vals));
		return -1;
	}
	return 0;
}
/*
 * Function Name:    oem_dell_lcd_get_single_line_text
 *
 * Description:    This function updates current lcd configuration
 * Input:          intf            - ipmi interface
 *                 lcdstring       - new string to be updated
 *                 max_length      - length of the string
 * Output:
 * Return:
 */
static
int oem_dell_lcd_get_single_line_text(struct ipmi_intf *intf, char *lcdstring, 
							uint8_t max_length)
{
	ipmi_dell_lcd_string lcdstringblock = {0};
	int lcdstring_len = 0;
	int bytes_copied = 0;
	int i = 0, rc = 0;
	for(i = 0; i < 4; i++) {
		int bytes_to_copy;
		rc = ipmi_mc_getsysinfo(intf, IPMI_DELL_LCD_STRING_SELECTOR, i, 0, 
				sizeof(lcdstringblock), &lcdstringblock);
		if(rc < 0) {
			lprintf(LOG_ERR, 
					"Error getting text data");
			return -1;
		} else if( rc ) {
			lprintf(LOG_ERR, 
					"Error getting text data: %s", 
					val2str(rc, completion_code_vals));
			return -1;
		}
		/* first block is different - 14 bytes*/
		if(0 == i) {
			lcdstring_len = lcdstringblock.lcd_string.selector_0_string.length;
			if(lcdstring_len < 1 || lcdstring_len > max_length) {
				break;
			}
			bytes_to_copy = MIN(lcdstring_len, IPMI_DELL_LCD_STRING1_SIZE);
			memcpy(lcdstring, lcdstringblock.lcd_string.selector_0_string.data, 
					bytes_to_copy);
		} else{
			int string_offset;
			bytes_to_copy = MIN(lcdstring_len - bytes_copied, 
					IPMI_DELL_LCD_STRINGN_SIZE);
			if(bytes_to_copy < 1) {
				break;
			}
			string_offset = IPMI_DELL_LCD_STRING1_SIZE + IPMI_DELL_LCD_STRINGN_SIZE
				*(i - 1);
			memcpy(lcdstring+string_offset, 
					lcdstringblock.lcd_string.selector_n_data, bytes_to_copy);
		}
		bytes_copied += bytes_to_copy;
		if(bytes_copied >= lcdstring_len) {
			break;
		}
	}
	return 0;
}
/*
 * Function Name:    oem_dell_lcd_get_info_wh
 *
 * Description:     This function prints current lcd configuration for whoville platform
 * Input:           intf            - ipmi interface
 * Output:
 * Return:
 */
static
int oem_dell_lcd_get_info_wh(struct ipmi_intf *intf)
{
	ipmi_dell_lcd_caps lcd_caps = {0};
	char lcdstring[IPMI_DELL_LCD_STRING_LENGTH_MAX+1] = {0};
	int rc;
	printf("LCD info\n");
	if(oem_dell_lcd_get_configure_command_wh(intf) != 0) {
		return -1;
	}
	if(lcd_mode.lcdmode == IPMI_DELL_LCD_CONFIG_DEFAULT) {
		char text[IPMI_DELL_LCD_STRING_LENGTH_MAX+1] = {0};
		if(ipmi_lcd_get_platform_model_name(intf, text, 
					IPMI_DELL_LCD_STRING_LENGTH_MAX, 
					IPMI_DELL_PLATFORM_MODEL_NAME_SELECTOR) != 0) {
			return(-1);
		}
		printf("    Setting:Model name\n");
		printf("    Line 1:  %s\n", text);
	} else if(lcd_mode.lcdmode == IPMI_DELL_LCD_CONFIG_NONE) {
		printf("    Setting:   none\n");
	} else if(lcd_mode.lcdmode == IPMI_DELL_LCD_CONFIG_USER_DEFINED) {
		printf("    Setting: User defined\n");
		rc = ipmi_mc_getsysinfo(intf, IPMI_DELL_LCD_GET_CAPS_SELECTOR, 0, 0, 
				sizeof(lcd_caps), &lcd_caps);
		if(rc < 0) {
			lprintf(LOG_ERR, 
					"Error getting LCD capabilities.");
			return -1;
		} else if((rc == IPMI_CC_INV_CMD) || (rc == IPMI_CC_REQ_DATA_NOT_PRESENT)) {
			lprintf(LOG_ERR, 
					"Error getting LCD capabilities: "
					"Command not supported on this system.");
		} else if( rc ) {
			lprintf(LOG_ERR, 
					"Error getting LCD capabilities: %s", 
					val2str(rc, completion_code_vals));
			return -1;
		}
		if(lcd_caps.number_lines > 0) {
			memset(lcdstring, 0, IPMI_DELL_LCD_STRING_LENGTH_MAX + 1);
			rc = oem_dell_lcd_get_single_line_text(intf, lcdstring, 
					lcd_caps.max_chars[0]);
			printf("    Text:    %s\n", lcdstring);
		} else{
			printf("    No lines to show\n");
		}
	} else if(lcd_mode.lcdmode == IPMI_DELL_LCD_iDRAC_IPV4ADRESS) {
		printf("    Setting:   IPV4 Address\n");
	} else if(lcd_mode.lcdmode == IPMI_DELL_LCD_IDRAC_MAC_ADDRESS) {
		printf("    Setting:   MAC Address\n");
	} else if(lcd_mode.lcdmode == IPMI_DELL_LCD_OS_SYSTEM_NAME) {
		printf("    Setting:   OS System Name\n");
	} else if(lcd_mode.lcdmode == IPMI_DELL_LCD_SERVICE_TAG) {
		printf("    Setting:   System Tag\n");
	} else if(lcd_mode.lcdmode == IPMI_DELL_LCD_iDRAC_IPV6ADRESS) {
		printf("    Setting:  IPV6 Address\n");
	} else if(lcd_mode.lcdmode == IPMI_DELL_LCD_ASSET_TAG) {
		printf("    Setting:  Asset Tag\n");
	} else if(lcd_mode.lcdmode == IPMI_DELL_LCD_AMBEINT_TEMP) {
		printf("    Setting:  Ambient Temp\n");
		if(lcd_mode.lcdquallifier & 0x02) {
			printf("    Unit:  F\n");
		} else{
			printf("    Unit:  C\n");
		}
	} else if(lcd_mode.lcdmode == IPMI_DELL_LCD_SYSTEM_WATTS) {
		printf("    Setting:  System Watts\n");
		if(lcd_mode.lcdquallifier & 0x01) {
			printf("    Unit:  BTU/hr\n");
		} else{
			printf("    Unit:  Watt\n");
		}
	}
	if(lcd_mode.error_display == IPMI_DELL_LCD_ERROR_DISP_SEL) {
		printf("    Error Display:  SEL\n");
	} else if(lcd_mode.error_display == IPMI_DELL_LCD_ERROR_DISP_VERBOSE) {
		printf("    Error Display:  Simple\n");
	}
	return 0;
}
/*
 * Function Name:    oem_dell_lcd_get_info
 *
 * Description:      This function prints current lcd configuration for platform other than whoville
 * Input:            intf            - ipmi interface
 * Output:
 * Return:
 */
static
int oem_dell_lcd_get_info(struct ipmi_intf *intf)
{
	ipmi_dell_lcd_caps lcd_caps = {0};
	uint8_t command = 0;
	char lcdstring[IPMI_DELL_LCD_STRING_LENGTH_MAX+1] = {0};
	int rc = -1;

	printf("LCD info\n");

	if(oem_dell_lcd_get_configure_command(intf, &command) != 0) {
		return -1;
	}
	if(command == IPMI_DELL_LCD_CONFIG_DEFAULT) {
		memset(lcdstring, 0, IPMI_DELL_LCD_STRING_LENGTH_MAX+1);
		if(ipmi_lcd_get_platform_model_name(intf, lcdstring, 
					IPMI_DELL_LCD_STRING_LENGTH_MAX, 
					IPMI_DELL_PLATFORM_MODEL_NAME_SELECTOR) != 0) {
			return(-1);
		}
		printf("    Setting: default\n");
		printf("    Line 1:  %s\n", lcdstring);
	} else if(command == IPMI_DELL_LCD_CONFIG_NONE) {
		printf("    Setting:   none\n");
	} else if(command == IPMI_DELL_LCD_CONFIG_USER_DEFINED) {
		printf("    Setting: custom\n");
		rc = ipmi_mc_getsysinfo(intf, IPMI_DELL_LCD_GET_CAPS_SELECTOR, 0, 0, 
				sizeof(lcd_caps), &lcd_caps);
		if(rc < 0) {
			lprintf(LOG_ERR, 
					"Error getting LCD capabilities.");
			return -1;
		} else if((rc == IPMI_CC_INV_CMD) || (rc == IPMI_CC_REQ_DATA_NOT_PRESENT)) {
			lprintf(LOG_ERR, 
					"Error getting LCD capabilities: "
					"Command not supported on this system.");
		} else if( rc ) {
			lprintf(LOG_ERR, 
					"Error getting LCD capabilities: %s", 
					val2str(rc, completion_code_vals));
			return -1;
		}
		if(lcd_caps.number_lines > 0) {
			memset(lcdstring, 0, IPMI_DELL_LCD_STRING_LENGTH_MAX + 1);
			rc = oem_dell_lcd_get_single_line_text(intf, lcdstring, 
					lcd_caps.max_chars[0]);
			printf("    Text:    %s\n", lcdstring);
		} else{
			printf("    No lines to show\n");
		}
	}
	return 0;
}

/*
 * Function Name:    oem_dell_lcd_get_status_val
 *
 * Description:      This function gets current lcd configuration
 * Input:            intf            - ipmi interface
 * Output:           lcdstatus       - KVM Status & Lock Status
 * Return:
 */
static
int oem_dell_lcd_get_status_val(struct ipmi_intf *intf, lcd_status *lcdstatus)
{
	int rc;
	rc = ipmi_mc_getsysinfo(intf, IPMI_DELL_LCD_STATUS_SELECTOR, 0, 0, 
			sizeof(*lcdstatus), lcdstatus);
	if(rc < 0) {
		lprintf(LOG_ERR, 
				"Error getting LCD Status");
		return -1;
	} else if((rc == IPMI_CC_INV_CMD) || (rc == IPMI_CC_REQ_DATA_NOT_PRESENT)) {
		lprintf(LOG_ERR, 
				"Error getting LCD status: "
				"Command not supported on this system.");
		return -1;
	} else if( rc ) {
		lprintf(LOG_ERR, 
				"Error getting LCD Status: %s", 
				val2str(rc, completion_code_vals));
		return -1;
	}
	return 0;
}

/*
 * Function Name:    oem_dell_IsLCDSupported
 *
 * Description:   This function returns whether lcd supported or not
 * Input:
 * Output:
 * Return:
 */
static
int oem_dell_is_lcd_supported(void)
{
	return lcdsupported;
}

/*
 * Function Name:         oem_dell_CheckLCDSupport
 *
 * Description:  This function checks whether lcd supported or not
 * Input:        intf            - ipmi interface
 * Output:
 * Return:
 */
static
void oem_dell_check_lcd_support(struct ipmi_intf *intf)
{
	int rc = -1;
	lcdsupported = 0;
	rc = ipmi_mc_getsysinfo(intf, IPMI_DELL_LCD_STATUS_SELECTOR, 0, 0, 0, NULL);
	if(rc == 0) {
		lcdsupported = 1;
	}
}

/*
 * Function Name:     oem_dell_ipmi_lcd_status_print
 *
 * Description:    This function prints current lcd configuration KVM Status & Lock Status
 * Input:          lcdstatus - KVM Status & Lock Status
 * Output:
 * Return:
 */
static
void oem_dell_ipmi_lcd_status_print(lcd_status lcdstatus)
{
	switch(lcdstatus.vkvm_status) {
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
	switch(lcdstatus.lock_status) {
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
 * Function Name:     oem_dell_lcd_get_status
 *
 * Description:      This function gets current lcd KVM active status & lcd access mode
 * Input:            intf            - ipmi interface
 * Output:
 * Return:           -1 on error
 *                   0 if successful
 */
static
int oem_dell_lcd_get_status(struct ipmi_intf *intf)
{
	int rc = 0;
	lcd_status  lcdstatus = {0};
	rc = oem_dell_lcd_get_status_val( intf, &lcdstatus);
	if(rc < 0) {
		return -1;
	}
	oem_dell_ipmi_lcd_status_print(lcdstatus);
	return rc;
}

/*
 * Function Name:     oem_dell_lcd_set_kvm
 *
 * Description:       This function sets lcd KVM active status
 * Input:             intf            - ipmi interface
 *                    status  - Inactive / Active
 * Output:
 * Return:            -1 on error
 *                    0 if successful
 */
static
int oem_dell_lcd_set_kvm(struct ipmi_intf *intf, char status)
{
#define DATA_LEN 5
	lcd_status lcdstatus;
	int rc = 0;
	struct ipmi_rs *rsp = NULL;
	struct ipmi_rq req = {0};
	uint8_t data[5] = {0};
	uint8_t input_length = 0;
	rc = oem_dell_lcd_get_status_val(intf, &lcdstatus);
	if(rc < 0) {
		return -1;
	}
	req.msg.netfn = IPMI_NETFN_APP;
	req.msg.lun = 0;
	req.msg.cmd = IPMI_SET_SYS_INFO;
	req.msg.data_len = 5;
	req.msg.data = data;
	data[input_length++] = IPMI_DELL_LCD_STATUS_SELECTOR;
	data[input_length++] = status; /* active- incative*/
	data[input_length++] = lcdstatus.lock_status; /* full-veiw-locked */
	rsp = intf->sendrecv(intf, &req);
	if(NULL == rsp) {
		lprintf(LOG_ERR, 
				"Error setting LCD status");
		rc = -1;
	} else if((rsp->ccode == IPMI_CC_INV_CMD) || (rsp->ccode == IPMI_CC_REQ_DATA_NOT_PRESENT)) {
		lprintf(LOG_ERR, 
				"Error getting LCD status: "
				"Command not supported on this system.");
		return -1;
	} else if( rsp->ccode ) {
		lprintf(LOG_ERR, 
				"Error setting LCD status: %s", 
				val2str(rsp->ccode, completion_code_vals));
		rc = -1;
	}
	return rc;
}

/*
 * Function Name:   oem_dell_lcd_set_lock
 *
 * Description:     This function sets lcd access mode
 * Input:           intf            - ipmi interface
 *                  lock    - View and modify / View only / Diabled
 * Output:
 * Return:          -1 on error
 *                  0 if successful
 */
static
int oem_dell_lcd_set_lock(struct ipmi_intf *intf,  char lock)
{
	lcd_status lcdstatus;
	int rc = 0;
	struct ipmi_rs *rsp = NULL;
	struct ipmi_rq req = {0};
	uint8_t data[5];
	uint8_t input_length = 0 ;
	rc = oem_dell_lcd_get_status_val(intf, &lcdstatus);
	if(rc < 0) {
		return -1;
	}
	req.msg.netfn = IPMI_NETFN_APP;
	req.msg.lun = 0;
	req.msg.cmd = IPMI_SET_SYS_INFO;
	req.msg.data_len = 5;
	req.msg.data = data;
	data[input_length++] = IPMI_DELL_LCD_STATUS_SELECTOR;
	data[input_length++] = lcdstatus.vkvm_status; /* active- incative */
	data[input_length++] = lock; /* full- veiw-locked */
	rsp = intf->sendrecv(intf, &req);
	if(NULL == rsp) {
		lprintf(LOG_ERR, 
				"Error setting LCD status");
		rc = -1;
	} else if((rsp->ccode == IPMI_CC_INV_CMD) || (rsp->ccode == IPMI_CC_REQ_DATA_NOT_PRESENT)) {
		lprintf(LOG_ERR, 
				"Error getting LCD status: "
				"Command not supported on this system.");
		rc = -1;
	} else if( rsp->ccode ) {
		lprintf(LOG_ERR, 
				"Error setting LCD status: %s", 
				val2str(rsp->ccode, completion_code_vals));
		rc = -1;
	}
	return rc;
}

/*
 * Function Name:   oem_dell_lcd_set_single_line_text
 *
 * Description:    This function sets lcd line text
 * Input:          intf            - ipmi interface
 *                 text    - lcd string
 * Output:
 * Return:         -1 on error
 *                 0 if successful
 */
static
int oem_dell_lcd_set_single_line_text(struct ipmi_intf *intf, char *text)
{
	uint8_t data[18];
	uint8_t input_length = 0;
	int bytes_to_store = strlen(text);
	int bytes_stored = 0;
	int i;
	int rc = 0;
	if(bytes_to_store > IPMI_DELL_LCD_STRING_LENGTH_MAX) {
		lprintf(LOG_ERR, 
				"Out of range Max limit is 62 characters");
		return(-1);
	} else{
		bytes_to_store = MIN(bytes_to_store, IPMI_DELL_LCD_STRING_LENGTH_MAX);
		for(i = 0; i < 4; i++) {
			/*first block, 2 bytes parms and 14 bytes data*/
			input_length = 0;
			if(0 == i) {
				int size_of_copy = MIN((bytes_to_store - bytes_stored), 
						IPMI_DELL_LCD_STRING1_SIZE);
				if(size_of_copy < 0) {
					/* allow 0 string length*/
					break;
				}
				data[input_length++] = IPMI_DELL_LCD_STRING_SELECTOR;
				data[input_length++] = i; /* block number to use(0)*/
				data[input_length++] = 0; /*string encoding*/
				data[input_length++] = bytes_to_store; /* total string length*/
				memcpy(data + 4, text+bytes_stored, size_of_copy);
				bytes_stored += size_of_copy;
			} else{
				int size_of_copy = MIN((bytes_to_store - bytes_stored), 
						IPMI_DELL_LCD_STRINGN_SIZE);
				if(size_of_copy <= 0) {
					break;
				}
				data[input_length++] = IPMI_DELL_LCD_STRING_SELECTOR;
				data[input_length++] = i; /* block number to use(1, 2, 3)*/
				memcpy(data + 2, text+bytes_stored, size_of_copy);
				bytes_stored += size_of_copy;
			}
			rc = ipmi_mc_setsysinfo(intf, 18, data);
			if(rc < 0) {
				lprintf(LOG_ERR, 
						"Error setting text data");
				rc = -1;
			} else if( rc ) {
				lprintf(LOG_ERR, 
						"Error setting text data: %s", 
						val2str(rc, completion_code_vals));
				rc = -1;
			}
		}
	}
	return rc;
}

/*
 * Function Name:   oem_dell_lcd_set_text
 *
 * Description:     This function sets lcd line text
 * Input:           intf            - ipmi interface
 *                  text    - lcd string
 *                  line_number- line number
 * Output:
 * Return:          -1 on error
 *                  0 if successful
 */
static
int oem_dell_lcd_set_text(struct ipmi_intf *intf, char *text, int line_number)
{
	int rc = 0;
	ipmi_dell_lcd_caps lcd_caps;
	rc = ipmi_mc_getsysinfo(intf, IPMI_DELL_LCD_GET_CAPS_SELECTOR, 0, 0, 
			sizeof(lcd_caps), &lcd_caps);
	if(rc < 0) {
		lprintf(LOG_ERR, 
				"Error getting LCD capabilities");
		return -1;
	} else if(rc > 0) {
		lprintf(LOG_ERR, 
				"Error getting LCD capabilities: %s", 
				val2str(rc, completion_code_vals));
		return -1;
	}
	if( lcd_caps.number_lines ) {
		rc = oem_dell_lcd_set_single_line_text(intf, text);
	} else{
		lprintf(LOG_ERR, 
				"LCD does not have any lines that can be set");
		rc = -1;
	}
	return rc;
}

/*
 * Function Name:   oem_dell_lcd_configure_wh
 *
 * Description:     This function updates the current lcd configuration
 * Input:           intf            - ipmi interface
 *                  lcdquallifier- lcd quallifier
 *                  errordisp       - error number
 *                  line_number-line number
 *                  text            - lcd string
 * Output:
 * Return:          -1 on error
 *                  0 if successful
 */
static
int oem_dell_lcd_configure_wh(struct ipmi_intf *intf, uint32_t  mode, 
				uint16_t lcdquallifier, uint8_t errordisp, int8_t line_number, char *text)
{
	int rc = 0;
	if(IPMI_DELL_LCD_CONFIG_USER_DEFINED == mode) {
		/* Any error was reported earlier. */
		rc = oem_dell_lcd_set_text(intf, text, line_number);
	}
	if(rc == 0) {
		rc = oem_dell_lcd_set_configure_command_wh(intf, mode , lcdquallifier, errordisp);
	}
	return rc;
}

/*
 * Function Name:   oem_dell_lcd_configure
 *
 * Description:     This function updates the current lcd configuration
 * Input:           intf            - ipmi interface
 *                  command- lcd command
 *                  line_number-line number
 *                  text            - lcd string
 * Output:
 * Return:          -1 on error
 *                  0 if successful
 */
static
int oem_dell_lcd_configure(struct ipmi_intf *intf, int command, 
				int8_t line_number, char *text)
{
	int rc = 0;
	if(IPMI_DELL_LCD_CONFIG_USER_DEFINED == command) {
		rc = oem_dell_lcd_set_text(intf, text, line_number);
	}
	if(rc == 0) {
		rc = oem_dell_lcd_set_configure_command(intf, command);
	}
	return rc;
}

/*
 * Function Name:   oem_dell_lcd_usage
 *
 * Description:   This function prints help message for lcd command
 * Input:
 * Output:
 *
 * Return:
 */
static
void oem_dell_lcd_usage(void)
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
			"   Allows you to set the LCD display mode to any of the preceding");
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
 * Function Name:       oem_dell_mac_main
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
static
int oem_dell_mac_main(struct ipmi_intf *intf, int argc, char **argv)
{
	int rc = 0;
	int currIdInt = -1;
	current_arg++;
	if((argc == 1) || (argc > 1 && strcmp(argv[current_arg], "help") == 0)) {
		oem_dell_mac_usage();
		return 0;
	}
	if(strcmp(argv[current_arg], "list") == 0) {
		rc = oem_dell_macinfo(intf, 0xff);
	} else if(strcmp(argv[current_arg], "get") == 0) {
		current_arg++;
		if(NULL == argv[current_arg] ) {
			oem_dell_mac_usage();
			return -1;
		}
		if(str2int(argv[current_arg], &currIdInt) != 0) {
			lprintf(LOG_ERR, 
					"Invalid NIC number. The NIC number should be between 0-8");
			return -1;
		}
		if((currIdInt > 8) || (currIdInt < 0)) {
			lprintf(LOG_ERR, 
					"Invalid NIC number. The NIC number should be between 0-8");
			return -1;
		}
		rc = oem_dell_macinfo(intf, currIdInt);
	} else{
		oem_dell_mac_usage();
	}
	return rc;
}

embedded_nic_mac_address_type embedded_nic_mac_address;

embedded_nic_mac_address_type_10g embedded_nic_mac_address_10g;

static
void oem_dell_init_embedded_nic_mac_address_values()
{
	uint8_t i;
	uint8_t j;
	for(i = 0; i < MAX_LOM; i++) {
		embedded_nic_mac_address.lom_mac_address[i].bladslotnumber = 0;
		embedded_nic_mac_address.lom_mac_address[i].mactype = LOM_MACTYPE_RESERVED;
		embedded_nic_mac_address.lom_mac_address[i].ethernetstatus = 
			LOM_ETHERNET_RESERVED;
		embedded_nic_mac_address.lom_mac_address[i].nicnumber = 0;
		embedded_nic_mac_address.lom_mac_address[i].reserved = 0;
		for(j = 0; j < MACADDRESSLENGH; j++) {
			embedded_nic_mac_address.lom_mac_address[i].macaddressbyte[j] = 0;
			embedded_nic_mac_address_10g.mac_address[i].mac_address_byte[j] = 0;
		}
	}
}

uint8_t usevirtualmacaddress = 0;

static
int oem_dell_macinfo_drac_idrac_virtual_mac(struct ipmi_intf *intf, uint8_t nicnum)
{
	struct ipmi_rs *rsp;
	struct ipmi_rq req;
	uint8_t msg_data[30];
	uint8_t virtualmacaddress[MACADDRESSLENGH];
	uint8_t input_length = 0;
	uint8_t j;
	uint8_t i;
	if(nicnum != 0xff && nicnum != IDRAC_NIC_NUMBER) {
		return 0;
	}
	usevirtualmacaddress = 0;
	input_length = 0;
	msg_data[input_length++] = 1; /*Get*/

	req.msg.netfn = DELL_OEM_NETFN;
	req.msg.lun = 0;
	req.msg.cmd = GET_IDRAC_VIRTUAL_MAC;
	req.msg.data = msg_data;
	req.msg.data_len = input_length;

	rsp = intf->sendrecv(intf, &req);
	if(NULL == rsp) {
		return -1;
	}
	if( rsp->ccode ) {
		return -1;
	}
	if((IMC_IDRAC_12G_MODULAR == IMC_Type)
			||(IMC_IDRAC_12G_MONOLITHIC == IMC_Type)
			||(IMC_IDRAC_12G_MODULAR == IMC_Type)
			||(IMC_IDRAC_13G_MODULAR == IMC_Type)
			||(IMC_IDRAC_13G_MONOLITHIC == IMC_Type)
			||(IMC_IDRAC_13G_DCS == IMC_Type)
			||(IMC_IDRAC_14G_MODULAR == IMC_Type)
			||(IMC_IDRAC_14G_MONOLITHIC == IMC_Type)
			||(IMC_IDRAC_14G_DCS == IMC_Type)) {
		/* Get the Chasiss Assigned MAC Addresss for 12g and later Only */
		memcpy(virtualmacaddress, ((rsp->data) + VIRTUAL_MAC_OFFSET_12G_AND_LATER), MACADDRESSLENGH);
		for(i = 0; i < MACADDRESSLENGH; i++) {
			if(virtualmacaddress[i] != 0) {
				usevirtualmacaddress = 1;
			}
		}
		/* Get the Server Assigned MAC Addresss for 12g and later Only */
		if(!usevirtualmacaddress) {
			memcpy(virtualmacaddress, ((rsp->data) + VIRTUAL_MAC_OFFSET_12G_AND_LATER + MACADDRESSLENGH), 
					MACADDRESSLENGH);
			for(i = 0; i < MACADDRESSLENGH; i++) {
				if(virtualmacaddress[i] != 0) {
					usevirtualmacaddress = 1;
				}
			}
		}
	} else{
		memcpy(virtualmacaddress, ((rsp->data) + VIRTUAL_MAC_OFFSET), 
				MACADDRESSLENGH);
		for(i = 0; i < MACADDRESSLENGH; i++) {
			if(virtualmacaddress[i] != 0) {
				usevirtualmacaddress = 1;
			}
		}
	}
	if(usevirtualmacaddress == 0) {
		return -1;
	}
	if(IMC_IDRAC_10G == IMC_Type) {
		printf("\nDRAC MAC Address ");
	} else if((IMC_IDRAC_11G_MODULAR == IMC_Type)
		||(IMC_IDRAC_11G_MONOLITHIC == IMC_Type)) {
		printf("\niDRAC6 MAC Address ");
	} else if((IMC_IDRAC_12G_MODULAR == IMC_Type)
		||(IMC_IDRAC_12G_MONOLITHIC == IMC_Type)) {
		printf("\niDRAC7 MAC Address ");
	} else if((IMC_IDRAC_13G_MODULAR == IMC_Type)
		||(IMC_IDRAC_13G_MONOLITHIC == IMC_Type)
		||(IMC_IDRAC_13G_MODULAR == IMC_Type)) {
		printf("\niDRAC8 MAC Address ");
	} else if((IMC_IDRAC_14G_MODULAR == IMC_Type)
		||(IMC_IDRAC_14G_MONOLITHIC == IMC_Type)
		||(IMC_IDRAC_14G_MODULAR == IMC_Type)) {
		printf("\niDRAC9 MAC Address ");
	} else if((IMC_MASER_LITE_BMC == IMC_Type)
		||(IMC_MASER_LITE_NU == IMC_Type)) {
		printf("\nBMC MAC Address ");
	} else{
		printf("\niDRAC6 MAC Address ");
	}

	for(j = 0; j < 5; j++) {
		printf("%02x:", virtualmacaddress[j]);
	}
	printf("%02x", virtualmacaddress[j]);
	printf("\n");

	return 0;
}

/*
 * Function Name:    oem_dell_macinfo_drac_idrac_mac
 *
 * Description:      This function retrieves the mac address of DRAC or iDRAC
 * Input:            nicnum
 * Output:
 * Return:
 */
static
int oem_dell_macinfo_drac_idrac_mac(struct ipmi_intf *intf, uint8_t nicnum)
{
	struct ipmi_rs *rsp;
	struct ipmi_rq req;
	uint8_t msg_data[30];
	uint8_t input_length = 0;
	uint8_t idrac6macaddressbyte[MACADDRESSLENGH];
	uint8_t j;
	oem_dell_macinfo_drac_idrac_virtual_mac(intf, nicnum);
	if((nicnum != 0xff && nicnum != IDRAC_NIC_NUMBER)
			|| usevirtualmacaddress != 0) {
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
	if(NULL == rsp) {
		lprintf(LOG_ERR, 
				"Error in getting MAC Address");
		return -1;
	}
	if( rsp->ccode ) {
		lprintf(LOG_ERR, 
				"Error in getting MAC Address(%s)", 
				val2str(rsp->ccode, completion_code_vals));
		return -1;
	}
	memcpy(idrac6macaddressbyte, ((rsp->data) + PARAM_REV_OFFSET), 
			MACADDRESSLENGH);

	if(IMC_IDRAC_10G == IMC_Type) {
		printf("\nDRAC MAC Address ");
	} else if((IMC_IDRAC_11G_MODULAR == IMC_Type)
			||(IMC_IDRAC_11G_MONOLITHIC == IMC_Type)) {
		printf("\niDRAC6 MAC Address ");
	} else if((IMC_IDRAC_12G_MODULAR == IMC_Type)
			||(IMC_IDRAC_12G_MONOLITHIC == IMC_Type)) {
		printf("\niDRAC7 MAC Address ");
	} else if((IMC_IDRAC_13G_MODULAR == IMC_Type)
			||(IMC_IDRAC_13G_MONOLITHIC == IMC_Type) 
			||(IMC_IDRAC_13G_DCS == IMC_Type)) { 
		printf("\niDRAC8 MAC Address ");
	} else if((IMC_IDRAC_14G_MODULAR == IMC_Type)
		||(IMC_IDRAC_14G_MONOLITHIC == IMC_Type)
		||(IMC_IDRAC_14G_MODULAR == IMC_Type)) {
		printf("\niDRAC9 MAC Address ");
	} else if((IMC_MASER_LITE_BMC == IMC_Type)
			||(IMC_MASER_LITE_NU == IMC_Type)) {
		printf("\n\rBMC MAC Address ");
	} else{
		printf("\niDRAC6 MAC Address ");
	}
	for(j = 0; j < 5; j++) {
		printf("%02x:", idrac6macaddressbyte[j]);
	}
	printf("%02x", idrac6macaddressbyte[j]);
	printf("\n");

	return 0;
}

/*
 * Function Name:    oem_dell_macinfo_10g
 *
 * Description:      This function retrieves the mac address of LOMs
 * Input:            intf      - ipmi interface
 *                   nicnum    - NIC number
 * Output:
 * Return:
 */
static
int oem_dell_macinfo_10g(struct ipmi_intf *intf, uint8_t nicnum)
{
	struct ipmi_rs *rsp;
	struct ipmi_rq req;
	uint8_t msg_data[30];
	uint8_t input_length = 0;
	uint8_t j;
	uint8_t i;
	uint8_t total_no_nics = 0;
	oem_dell_init_embedded_nic_mac_address_values();
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
	if(NULL == rsp) {
		lprintf(LOG_ERR, 
				"Error in getting MAC Address");
		return -1;
	}
	if( rsp->ccode ) {
		lprintf(LOG_ERR, 
				"Error in getting MAC Address(%s)", 
				val2str(rsp->ccode, completion_code_vals));
		return -1;
	}
	total_no_nics = (uint8_t)rsp->data[0 + PARAM_REV_OFFSET]; /* Byte 1: Total Number of Embedded NICs */
	if(IDRAC_NIC_NUMBER != nicnum) {
		if(0xff == nicnum) {
			printf("\nSystem LOMs");
		}
		printf("\nNIC Number\tMAC Address\n");
		memcpy(&embedded_nic_mac_address_10g, 
				((rsp->data) + PARAM_REV_OFFSET+TOTAL_N0_NICS_INDEX), 
				total_no_nics * MACADDRESSLENGH);
		/*Read the LOM type and Mac Addresses */
		for(i = 0; i < total_no_nics; i++) {
			if((0xff == nicnum) ||(i == nicnum)) {
				printf("\n%d", i);
				printf("\t\t");
				for(j = 0 ; j < 5; j++) {
					printf("%02x:", embedded_nic_mac_address_10g.mac_address[i].mac_address_byte[j]);
				}
				printf("%02x", embedded_nic_mac_address_10g.mac_address[i].mac_address_byte[j]);
			}
		}
		printf("\n");
	}
	oem_dell_macinfo_drac_idrac_mac(intf, nicnum);
	return 0;
}

/*
 * Function Name:      oem_dell_macinfo_11g
 *
 * Description:        This function retrieves the mac address of LOMs
 * Input:              intf - ipmi interface
 * Output:
 * Return:
 */
static
int oem_dell_macinfo_11g(struct ipmi_intf *intf, uint8_t nicnum)
{
	struct ipmi_rs *rsp;
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
	oem_dell_init_embedded_nic_mac_address_values();
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
	if(NULL == rsp) {
		lprintf(LOG_ERR, 
				"Error in getting MAC Address");
		return -1;
	}
	if( rsp->ccode ) {
		lprintf(LOG_ERR, 
				"Error in getting MAC Address(%s)", 
				val2str(rsp->ccode, completion_code_vals));
		return -1;
	}
	len = 8; /*eigher 8 or 16 */
	maxlen = (uint8_t)rsp->data[0 + PARAM_REV_OFFSET];
	loop_count = maxlen / len;
	if(IDRAC_NIC_NUMBER != nicnum) {
		if(0xff == nicnum) {
			printf("\nSystem LOMs");
		}
		printf("\nNIC Number\tMAC Address\t\tStatus\n");
		/*Read the LOM type and Mac Addresses */
		offset = 0;
		for(i = 0; i < loop_count; i++, offset = offset + len) {
			input_length = 4;
			msg_data[input_length++] = offset;
			msg_data[input_length++] = len;

			req.msg.netfn = IPMI_NETFN_APP;
			req.msg.lun = 0;
			req.msg.cmd = IPMI_GET_SYS_INFO;
			req.msg.data = msg_data;
			req.msg.data_len = input_length;

			rsp = intf->sendrecv(intf, &req);
			if(NULL == rsp) {
				lprintf(LOG_ERR, 
						"Error in getting MAC Address");
				return -1;
			}
			if( rsp->ccode ) {
				lprintf(LOG_ERR, 
						"Error in getting MAC Address(%s)", 
						val2str(rsp->ccode, completion_code_vals));
				return -1;
			}
			memcpy(&(embedded_nic_mac_address.lom_mac_address[i]), 
					((rsp->data)+PARAM_REV_OFFSET), len);

			embedded_nic_mac_address.lom_mac_address[i].bladslotnumber = *((rsp->data)+PARAM_REV_OFFSET) & 0x0F;
			embedded_nic_mac_address.lom_mac_address[i].mactype = *((rsp->data)+PARAM_REV_OFFSET) & 0xC0;
			embedded_nic_mac_address.lom_mac_address[i].ethernetstatus = *((rsp->data)+PARAM_REV_OFFSET) & 0x30;
			embedded_nic_mac_address.lom_mac_address[i].nicnumber = *((rsp->data)+PARAM_REV_OFFSET+1) & 0x1F;
			embedded_nic_mac_address.lom_mac_address[i].reserved = *((rsp->data)+PARAM_REV_OFFSET+1) & 0x70;

			memcpy(&(embedded_nic_mac_address.lom_mac_address[i].macaddressbyte), ((rsp->data)+PARAM_REV_OFFSET+2), len-2);
			if(LOM_MACTYPE_ETHERNET == embedded_nic_mac_address.lom_mac_address[i].mactype) {
				if((0xff == nicnum)
						||(nicnum == embedded_nic_mac_address.lom_mac_address[i].nicnumber)) {
					printf("\n%d", embedded_nic_mac_address.lom_mac_address[i].nicnumber);
					printf("\t\t");
					for(j = 0; j < 5; j++) {
						printf("%02x:", embedded_nic_mac_address.lom_mac_address[i].macaddressbyte[j]);
					}
					printf("%02x", embedded_nic_mac_address.lom_mac_address[i].macaddressbyte[j]);

					if(LOM_ETHERNET_ENABLED
							 == embedded_nic_mac_address.lom_mac_address[i].ethernetstatus) {
						printf("\tEnabled");
					} else{
						printf("\tDisabled");
					}
				}
			}
		}
		printf("\n");
	}
	oem_dell_macinfo_drac_idrac_mac(intf, nicnum);
	return 0;
}

/*
 * Function Name:      oem_dell_macinfo
 *
 * Description:     This function retrieves the mac address of LOMs
 * Input:           intf   - ipmi interface
 * Output:
 * Return:
 */
static
int oem_dell_macinfo(struct ipmi_intf *intf, uint8_t nicnum)
{
	if(IMC_IDRAC_10G == IMC_Type) {
		return oem_dell_macinfo_10g(intf, nicnum);
	} else if((IMC_IDRAC_11G_MODULAR == IMC_Type
				|| IMC_IDRAC_11G_MONOLITHIC == IMC_Type)
			||(IMC_IDRAC_12G_MODULAR == IMC_Type
				|| IMC_IDRAC_12G_MONOLITHIC == IMC_Type)
			||(IMC_IDRAC_13G_MODULAR == IMC_Type
				|| IMC_IDRAC_13G_MONOLITHIC == IMC_Type
				|| IMC_IDRAC_13G_DCS == IMC_Type)
			||(IMC_IDRAC_14G_MODULAR == IMC_Type
				|| IMC_IDRAC_14G_MONOLITHIC == IMC_Type
				|| IMC_IDRAC_14G_DCS == IMC_Type)
			||(IMC_MASER_LITE_NU == IMC_Type || IMC_MASER_LITE_BMC == IMC_Type)) {
		return oem_dell_macinfo_11g(intf, nicnum);
	} else{
		lprintf(LOG_ERR, 
				"Error in getting MAC Address : Not supported platform");
		return(-1);
	}
}

/*
 * Function Name:     oem_dell_mac_usage
 *
 * Description:   This function prints help message for mac command
 * Input:
 * Output:
 *
 * Return:
 */
static
void oem_dell_mac_usage(void)
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
 * Function Name:       oem_dell_lan_main
 *
 * Description:         This function processes the delloem lan command
 * Input:               intf    - ipmi interface
 *                      argc    - no of arguments
 *                      argv    - argument string array
 * Output:
 *
 * Return:              return code     0 - success
 *                         -1 - failure
 */
static
int oem_dell_lan_main(struct ipmi_intf *intf, int argc, char **argv)
{
	int rc = 0;
	int nic_selection = 0;
	char nic_set[2] = {0};
	current_arg++;
	if( NULL == argv[current_arg]  || strcmp(argv[current_arg], "help") == 0) {
		oem_dell_lan_usage();
		return 0;
	}
	oem_dell_idracvalidator_command(intf);
	if(!oem_dell_islansupported()) {
		lprintf(LOG_ERR, 
				"lan is not supported on this system.");
		return -1;
	} else if(strcmp(argv[current_arg], "set") == 0) {
		current_arg++;
		if( NULL == argv[current_arg] ) {
			oem_dell_lan_usage();
			return -1;
		}
		if(iDRAC_FLAG_12_13)  {
			g_modularsupportlom = get_modular_support_lom(intf);
			nic_selection = oem_dell_get_nic_selection_mode_12g(intf, current_arg, argv, 
					nic_set);
			if(INVALID == nic_selection) {
				oem_dell_lan_usage();
				return -1;
			} else if(INVAILD_FAILOVER_MODE == nic_selection) {
				lprintf(LOG_ERR, 
						INVAILD_FAILOVER_MODE_STRING);
				return(-1);
			} else if(INVAILD_FAILOVER_MODE_SETTINGS == nic_selection) {
				lprintf(LOG_ERR, 
						INVAILD_FAILOVER_MODE_SET);
				return(-1);
			} else if(INVAILD_SHARED_MODE == nic_selection) {
				lprintf(LOG_ERR, 
						INVAILD_SHARED_MODE_SET_STRING);
				return(-1);
			}
			rc = oem_dell_lan_set_nic_selection_12g(intf, nic_set);
		} else{
			nic_selection = oem_dell_get_nic_selection_mode(current_arg, argv);
			if(INVALID == nic_selection) {
				oem_dell_lan_usage();
				return -1;
			}
			if(IMC_IDRAC_11G_MODULAR == IMC_Type) {
				lprintf(LOG_ERR, 
						INVAILD_SHARED_MODE_SET_STRING);
				return(-1);
			}
			rc = oem_dell_lan_set_nic_selection(intf, nic_selection);
		}
		return 0;
	} else if(strcmp(argv[current_arg], "get") == 0) {
		g_modularsupportlom = get_modular_support_lom(intf);
		current_arg++;
		if( NULL == argv[current_arg] ) {
			rc = oem_dell_lan_get_nic_selection(intf);
			return rc;
		} else if(strcmp(argv[current_arg], "active") == 0) {
			rc = oem_dell_lan_get_active_nic(intf);
			return rc;
		} else{
			oem_dell_lan_usage();
		}
	} else{
		oem_dell_lan_usage();
		return -1;
	}
	return rc;
}

int 
get_modular_support_lom(struct ipmi_intf* intf)
{
	struct ipmi_rs * rsp;
	struct ipmi_rq req;

	uint8_t msg_data[30];
	uint8_t input_length = 0;

	req.msg.netfn = DELL_OEM_NETFN;
	req.msg.lun = 0;
	req.msg.cmd = GET_MISCELLANEOUS_CMD;
	msg_data[input_length++] = GET_COMMAND_ID;
	msg_data[input_length++] = GET_HWCAPABILITY_SUB_CMD;
	msg_data[input_length++] = 8;
	msg_data[input_length++] = 0;
	msg_data[input_length++] = 0;
	msg_data[input_length++] = 0;


	req.msg.data = msg_data;
	req.msg.data_len = input_length;

	rsp = intf->sendrecv(intf, &req);
	if(rsp == NULL)
	{
		lprintf(LOG_ERR, " Error in getting nic selection");
		return 0;
	}
	else if(rsp->ccode > 0)
	{
		lprintf(LOG_ERR, " Error in getting nic selection(%s) \n", 
				val2str(rsp->ccode, completion_code_vals) );
		return 0;
	}
	//lprintf(LOG_ERR, " 9 = %x 8 = %x \n", rsp->data[9], rsp->data[8]);
	return(rsp->data[8] & BIT(4));

}

static
int oem_dell_islansupported()
{
	if(IMC_IDRAC_11G_MODULAR == IMC_Type) {
		return 0;
	}
	return 1;
}

static
int oem_dell_get_nic_selection_mode_12g(struct ipmi_intf *intf, int current_arg, 
					char **argv, char *nic_set)
{
	/* First get the current settings. */
	struct ipmi_rs *rsp;
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
	if(NULL == rsp) {
		lprintf(LOG_ERR, 
				"Error in getting nic selection");
		return -1;
	} else if( rsp->ccode ) {
		lprintf(LOG_ERR, 
				"Error in getting nic selection(%s)", 
				val2str(rsp->ccode, completion_code_vals));
		return -1;
	}
	nic_set[0] = rsp->data[0];
	nic_set[1] = rsp->data[1];
	if(NULL != argv[current_arg]
			&& strcmp(argv[current_arg], "dedicated") == 0) {
		nic_set[0] = 1;
		nic_set[1] = 0;
		return 0;
	}
	if(NULL != argv[current_arg]
			&& strcmp(argv[current_arg], "shared") == 0) {
		/* placeholder */
	} else{
		return INVALID;
	}

	current_arg++;
	if(NULL != argv[current_arg]
			&& strcmp(argv[current_arg], "with") == 0) {
		/* placeholder */
	} else{
		return INVALID;
	}

	current_arg++;
	if(NULL != argv[current_arg]
			&& strcmp(argv[current_arg], "failover") == 0) {
		failover = 1;
	}
	if(failover) {
		current_arg++;
	}
	if(NULL != argv[current_arg]
			&& strcmp(argv[current_arg], "lom1") == 0) {
		if( !g_modularsupportlom && ((IMC_IDRAC_12G_MODULAR == IMC_Type)
			||(IMC_IDRAC_13G_MODULAR == IMC_Type) ||(IMC_IDRAC_14G_MODULAR == IMC_Type) ) ) {

			return INVAILD_SHARED_MODE;
		}
		if(failover) {
			if(nic_set[0] == 2) {
				return INVAILD_FAILOVER_MODE;
			} else if(nic_set[0] == 1) {
				return INVAILD_FAILOVER_MODE_SETTINGS;
			}
			nic_set[1] = 2;
		} else{
			nic_set[0] = 2;
			if(nic_set[1] == 2) {
				nic_set[1] = 0;
			}
		}
		return 0;
	} else if(NULL != argv[current_arg]
			&& strcmp(argv[current_arg], "lom2") == 0) {
		if( !g_modularsupportlom && ((IMC_IDRAC_12G_MODULAR == IMC_Type)
			||(IMC_IDRAC_13G_MODULAR == IMC_Type) ||(IMC_IDRAC_14G_MODULAR == IMC_Type) ) ) {
			return INVAILD_SHARED_MODE;
		}
		if(failover) {
			if(nic_set[0] == 3) {
				return INVAILD_FAILOVER_MODE;
			} else if(nic_set[0] == 1) {
				return INVAILD_FAILOVER_MODE_SETTINGS;
			}
			nic_set[1] = 3;
		} else{
			nic_set[0] = 3;
			if(nic_set[1] == 3) {
				nic_set[1] = 0;
			}
		}
		return 0;
	} else if(NULL != argv[current_arg]
			&& strcmp(argv[current_arg], "lom3") == 0) {
		if( !g_modularsupportlom &&((IMC_IDRAC_12G_MODULAR == IMC_Type)
			||(IMC_IDRAC_13G_MODULAR == IMC_Type) ||(IMC_IDRAC_14G_MODULAR == IMC_Type) ) ) {
			return INVAILD_SHARED_MODE;
		}
		if(failover) {
			if(nic_set[0] == 4) {
				return INVAILD_FAILOVER_MODE;
			} else if(nic_set[0] == 1) {
				return INVAILD_FAILOVER_MODE_SETTINGS;
			}
			nic_set[1] = 4;
		} else{
			nic_set[0] = 4;
			if(nic_set[1] == 4) {
				nic_set[1] = 0;
			}
		}
		return 0;
	} else if(NULL != argv[current_arg]
			&& strcmp(argv[current_arg], "lom4") == 0) {
		if( !g_modularsupportlom &&((IMC_IDRAC_12G_MODULAR == IMC_Type)
			||(IMC_IDRAC_13G_MODULAR == IMC_Type) ||(IMC_IDRAC_14G_MODULAR == IMC_Type) ) ) {
			return INVAILD_SHARED_MODE;
		}
		if(failover) {
			if(nic_set[0] == 5) {
				return INVAILD_FAILOVER_MODE;
			} else if(nic_set[0] == 1) {
				return INVAILD_FAILOVER_MODE_SETTINGS;
			}
			nic_set[1] = 5;
		} else{
			nic_set[0] = 5;
			if(nic_set[1] == 5) {
				nic_set[1] = 0;
			}
		}
		return 0;
	} else if(failover && NULL != argv[current_arg]
			&& strcmp(argv[current_arg], "none") == 0) {
		if( !g_modularsupportlom &&((IMC_IDRAC_12G_MODULAR == IMC_Type)
			||(IMC_IDRAC_13G_MODULAR == IMC_Type) ||(IMC_IDRAC_14G_MODULAR == IMC_Type) ) ) {
			return INVAILD_SHARED_MODE;
		}
		if(failover) {
			if(nic_set[0] == 1) {
				return INVAILD_FAILOVER_MODE_SETTINGS;
			}
			nic_set[1] = 0;
		}
		return 0;
	} else if(failover && NULL != argv[current_arg]
			&& strcmp(argv[current_arg], "all") == 0) {
		/* placeholder */
	} else{
		return INVALID;
	}

	current_arg++;
	if(failover && NULL != argv[current_arg]
			&& strcmp(argv[current_arg], "loms") == 0) {
		if( !g_modularsupportlom &&((IMC_IDRAC_12G_MODULAR == IMC_Type)
			||(IMC_IDRAC_13G_MODULAR == IMC_Type) ||(IMC_IDRAC_14G_MODULAR == IMC_Type) ) ) {
			return INVAILD_SHARED_MODE;
		}
		if(nic_set[0] == 1) {
			return INVAILD_FAILOVER_MODE_SETTINGS;
		}
		nic_set[1] = 6;
		return 0;
	}
	return INVALID;
}

static
int oem_dell_get_nic_selection_mode(int current_arg, char ** argv)
{
	if(NULL != argv[current_arg]
			&& strcmp(argv[current_arg], "dedicated") == 0) {
		return DEDICATED;
	}
	if(NULL != argv[current_arg]
			&& strcmp(argv[current_arg], "shared") == 0) {
		if( NULL == argv[current_arg+1] ) {
			return SHARED;
		}
	}

	current_arg++;
	if(NULL != argv[current_arg]
			&& strcmp(argv[current_arg], "with") == 0) {
		/* place holder */
	} else{
		return INVALID;
	}

	current_arg++;
	if(NULL != argv[current_arg]
			&& strcmp(argv[current_arg], "failover") == 0) {
		/* place holder */
	} else{
		return INVALID;
	}

	current_arg++;
	if(NULL != argv[current_arg]
			&& strcmp(argv[current_arg], "lom2") == 0) {
		return SHARED_WITH_FAILOVER_LOM2;
	} else if(NULL != argv[current_arg]
			&& strcmp(argv[current_arg], "all") == 0) {
		/* place holder */
	} else{
		return INVALID;
	}

	current_arg++;
	if(NULL != argv[current_arg]
			&& strcmp(argv[current_arg], "loms") == 0) {
		return SHARED_WITH_FAILOVER_ALL_LOMS;
	}
	return INVALID;
}

static
int oem_dell_lan_set_nic_selection_12g(struct ipmi_intf *intf, uint8_t *nic_selection)
{
	struct ipmi_rs *rsp;
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
	if(NULL == rsp) {
		lprintf(LOG_ERR, 
				"Error in setting nic selection");
		return -1;
	} else if((nic_selection[0] == 1)
			&&(( iDRAC_FLAG_12_13 )
				&&(rsp->ccode == LICENSE_NOT_SUPPORTED))) {
		/* Check license only for setting the dedicated nic. */
		lprintf(LOG_ERR, 
				"FM001 : A required license is missing or expired");
		return -1;
	} else if( rsp->ccode ) {
		lprintf(LOG_ERR, 
				"Error in setting nic selection(%s)", 
				val2str(rsp->ccode, completion_code_vals));
		return -1;
	}
	printf("configured successfully");
	return 0;
}

static
int oem_dell_lan_set_nic_selection(struct ipmi_intf *intf, uint8_t nic_selection)
{
	struct ipmi_rs *rsp;
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
	if(NULL == rsp) {
		lprintf(LOG_ERR, 
				"Error in setting nic selection");
		return -1;
	} else if( rsp->ccode ) {
		lprintf(LOG_ERR, 
				"Error in setting nic selection(%s)", 
				val2str(rsp->ccode, completion_code_vals));
		return -1;
	}
	printf("configured successfully");
	return 0;
}

static
int oem_dell_lan_get_nic_selection(struct ipmi_intf *intf)
{
	struct ipmi_rs *rsp;
	struct ipmi_rq req;
	uint8_t input_length = 0;
	uint8_t msg_data[30];
	uint8_t nic_selection = -1;
	uint8_t nic_selection_failover = 0;

	input_length = 0;
	req.msg.netfn = DELL_OEM_NETFN;
	req.msg.lun = 0;
	if((iDRAC_FLAG_12_13 ) 
		||(iDRAC_FLAG == IDRAC_13G) 
		||(iDRAC_FLAG == IDRAC_14G) ){
		req.msg.cmd = GET_NIC_SELECTION_12G_CMD;
	} else{
		req.msg.cmd = GET_NIC_SELECTION_CMD;
	}
	req.msg.data = msg_data;
	req.msg.data_len = input_length;
	rsp = intf->sendrecv(intf, &req);
	if(NULL == rsp) {
		lprintf(LOG_ERR, 
				"Error in getting nic selection");
		return -1;
	} else if( rsp->ccode ) {
		lprintf(LOG_ERR, 
				"Error in getting nic selection(%s)", 
				val2str(rsp->ccode, completion_code_vals));
		return -1;
	}
	nic_selection = rsp->data[0];
	if((iDRAC_FLAG_12_13 ) 
		||(iDRAC_FLAG == IDRAC_13G) 
		||(iDRAC_FLAG == IDRAC_14G)) {
		nic_selection_failover = rsp->data[1];
		if((nic_selection < MAX_NUM_LOMS_IDX) &&(nic_selection > 0) &&
                  (nic_selection_failover <(MAX_NUM_LOMS_IDX+1)) &&(nic_selection != RESERVED_LOM_IDX ))
		{
			if(nic_selection == 1) {
				printf("%s\n", nic_selection_mode_string_12g[nic_selection-1]);
			} else if(nic_selection) {
				printf("Shared LOM   :  %s\n", 
						nic_selection_mode_string_12g[nic_selection-1]);
				if(nic_selection_failover  == 0) {
					printf("Failover LOM :  None\n");
				} else if(nic_selection_failover <= FAILOVER_LOM_START_IDX && nic_selection_failover <= MAX_NUM_LOMS_IDX) {
					printf("Failover LOM :  %s\n", 
							nic_selection_mode_string_12g[nic_selection_failover + FAILOVER_LOM_STRING_IDX]);
				}
			}
		} else{
			lprintf(LOG_ERR, 
					"Error Outof bond Value received(%d)(%d)", 
					nic_selection, nic_selection_failover);
			return -1;
		}
	} else{
		printf("%s\n", nic_selection_mode_string[nic_selection]);
	}
	return 0;
}

static
int oem_dell_lan_get_active_nic(struct ipmi_intf *intf)
{
	struct ipmi_rs *rsp;
	struct ipmi_rq req;
	uint8_t active_nic = 0;
	uint8_t current_lom = 0;
	uint8_t input_length = 0;
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
	if(NULL == rsp) {
		lprintf(LOG_ERR, 
				"Error in getting Active LOM Status");
		return -1;
	} else if( rsp->ccode ) {
		lprintf(LOG_ERR, 
				"Error in getting Active LOM Status(%s)", 
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
	if(NULL == rsp) {
		lprintf(LOG_ERR, 
				"Error in getting Active LOM Status");
		return -1;
	} else if( rsp->ccode ) {
		lprintf(LOG_ERR, 
				"Error in getting Active LOM Status(%s)", 
				val2str(rsp->ccode, completion_code_vals));
		return -1;
	}
	active_nic = rsp->data[1];
	if(current_lom < MAX_NUM_LOMS_IDX && active_nic) {
		printf("\n%s\n", acivelom_string[current_lom]);
	} else{
		printf("\n%s\n", acivelom_string[0]);
	}
	return 0;
}

static
void oem_dell_lan_usage(void)
{
	lprintf(LOG_NOTICE, "");
	lprintf(LOG_NOTICE, "   lan set <Mode> ");
	if((iDRAC_FLAG == IDRAC_12G) ||(iDRAC_FLAG == IDRAC_13G) ||(iDRAC_FLAG == IDRAC_14G)) {
		lprintf(LOG_NOTICE, "    sets the NIC Selection Mode :");
		lprintf(LOG_NOTICE, "           dedicated, shared with lom1, shared with lom2, shared with lom3, shared");
		lprintf(LOG_NOTICE, "            with lom4");
		lprintf(LOG_NOTICE, "");
		lprintf(LOG_NOTICE, "   lan set <Shared Failover Mode> ");
		lprintf(LOG_NOTICE, "    sets the shared Failover Mode :");
		lprintf(LOG_NOTICE, "           shared with failover lom1, shared with failover lom2, shared with failo");
		lprintf(LOG_NOTICE, "           ver lom3, shared with failover lom4, shared with failover all loms, sha");
		lprintf(LOG_NOTICE, "           red with failover none.");
	} else{
		lprintf(LOG_NOTICE, "    sets the NIC Selection Mode:");
		lprintf(LOG_NOTICE, "       dedicated, shared, shared with failover lom2, shared with failover all l");
		lprintf(LOG_NOTICE, "       oms");
	}
	lprintf(LOG_NOTICE, "");
	lprintf(LOG_NOTICE, "   lan get ");
	if((iDRAC_FLAG == IDRAC_12G) ||(iDRAC_FLAG == IDRAC_13G) ||(iDRAC_FLAG == IDRAC_14G) ) {
		lprintf(LOG_NOTICE, "    Get the current NIC selection:");
		lprintf(LOG_NOTICE, "           dedicated, shared with lom1, shared with lom2, shared with lom3, shared");
		lprintf(LOG_NOTICE, "            lom4");
		lprintf(LOG_NOTICE, "    Get the current Shared failover modes:");
		lprintf(LOG_NOTICE, "           shared with failover lom1, shared with failover lom2, shared with failo");
		lprintf(LOG_NOTICE, "       ver lom3, shared with failover lom4, shared with failover all loms, sha");
		lprintf(LOG_NOTICE, "       red with failover none.");
	}else{
		lprintf(LOG_NOTICE, "    Get the current NIC Selection Mode:");
		lprintf(LOG_NOTICE, "           dedicated, shared, shared with failover lom2, shared with failover all l");
		lprintf(LOG_NOTICE, "           oms");

	}
	lprintf(LOG_NOTICE, "");
	lprintf(LOG_NOTICE, "   lan get active");
	lprintf(LOG_NOTICE, "      Get the current active LOMs(LOM1, LOM2, LOM3, LOM4, NONE).");
	lprintf(LOG_NOTICE, "");


}

/*
 * Function Name:       oem_dell_powermonitor_main
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
static
int oem_dell_powermonitor_main(struct ipmi_intf *intf, int argc, char **argv)
{
	int rc = 0;
	current_arg++;
	if(argc > 1 && strcmp(argv[current_arg], "help") == 0) {
		oem_dell_powermonitor_usage();
		return 0;
	}
	oem_dell_idracvalidator_command(intf);
	if(argc == 1) {
		rc = oem_dell_powermgmt(intf);
	} else if(strcmp(argv[current_arg], "status") == 0) {
		rc = oem_dell_powermgmt(intf);
	} else if(strcmp(argv[current_arg], "clear") == 0) {
		current_arg++;
		if( NULL == argv[current_arg] ) {
			oem_dell_powermonitor_usage();
			return -1;
		} else if(strcmp(argv[current_arg], "peakpower") == 0) {
			rc = oem_dell_powermgmt_clear(intf, 1);
		} else if(strcmp(argv[current_arg], "cumulativepower") == 0) {
			rc = oem_dell_powermgmt_clear(intf, 0);
		} else{
			oem_dell_powermonitor_usage();
			return -1;
		}
	} else if(strcmp(argv[current_arg], "powerconsumption") == 0) {
		current_arg++;
		if( NULL == argv[current_arg] ) {
			rc = oem_dell_print_get_power_consmpt_data(intf, watt);
		} else if(strcmp(argv[current_arg], "watt") == 0) {
			rc = oem_dell_print_get_power_consmpt_data(intf, watt);
		} else if(strcmp(argv[current_arg], "btuphr") == 0) {
			rc = oem_dell_print_get_power_consmpt_data(intf, btuphr);
		} else{
			oem_dell_powermonitor_usage();
			return -1;
		}
	} else if(strcmp(argv[current_arg], "powerconsumptionhistory") == 0) {
		current_arg++;
		if( NULL == argv[current_arg] ) {
			rc = oem_dell_print_power_consmpt_history(intf, watt);
		} else if(strcmp(argv[current_arg], "watt") == 0) {
			rc = oem_dell_print_power_consmpt_history(intf, watt);
		} else if(strcmp(argv[current_arg], "btuphr") == 0) {
			rc = oem_dell_print_power_consmpt_history(intf, btuphr);
		} else{
			oem_dell_powermonitor_usage();
			return -1;
		}
	} else if(strcmp(argv[current_arg], "getpowerbudget") == 0) {
		current_arg++;
		if( NULL == argv[current_arg] ) {
			rc = oem_dell_print_power_cap(intf, watt);
		} else if(strcmp(argv[current_arg], "watt") == 0) {
			rc = oem_dell_print_power_cap(intf, watt);
		} else if(strcmp(argv[current_arg], "btuphr") == 0) {
			rc = oem_dell_print_power_cap(intf, btuphr);
		} else{
			oem_dell_powermonitor_usage();
			return -1;
		}
	} else if(strcmp(argv[current_arg], "setpowerbudget") == 0) {
		int val;
		current_arg++;
		if( NULL == argv[current_arg] ) {
			oem_dell_powermonitor_usage();
			return -1;
		}
		if(strchr(argv[current_arg], '.')) {
			lprintf(LOG_ERR, 
					"Cap value in Watts, Btu/hr or percent should be whole number");
			return -1;
		}
		if(str2int(argv[current_arg], &val) != 0) {
			lprintf(LOG_ERR, 
					"Given capacity value '%s' is invalid.", 
					argv[current_arg]);
			return(-1);
		}
		current_arg++;
		if( NULL == argv[current_arg] ) {
			oem_dell_powermonitor_usage();
		} else if(strcmp(argv[current_arg], "watt") == 0) {
			rc = oem_dell_set_power_cap(intf, watt, val);
		} else if(strcmp(argv[current_arg], "btuphr") == 0) {
			rc = oem_dell_set_power_cap(intf, btuphr, val);
		} else if(strcmp(argv[current_arg], "percent") == 0) {
			rc = oem_dell_set_power_cap(intf, percent, val);
		} else{
			oem_dell_powermonitor_usage();
			return -1;
		}
	} else if(strcmp(argv[current_arg], "enablepowercap") == 0) {
		oem_dell_set_power_capstatus_command(intf, 1);
	} else if(strcmp(argv[current_arg], "disablepowercap") == 0) {
		oem_dell_set_power_capstatus_command(intf, 0);
	} else{
		oem_dell_powermonitor_usage();
		return -1;
	}
	return rc;
}
/*
 * Function Name:     oem_dell_time_to_str
 *
 * Description:       This function converts ipmi time format into gmtime format
 * Input:             rawTime  - ipmi time format
 * Output:            strTime  - gmtime format
 *
 * Return:
 */
static
void oem_dell_time_to_str(time_t rawTime, char *strTime)
{
	struct tm *tm;
	char *temp;
	tm = gmtime(&rawTime);
	temp = asctime(tm);
	strcpy(strTime, temp);
}
/*
 * Function Name:      oem_dell_get_sensor_reading
 *
 * Description:        This function retrieves a raw sensor reading
 * Input:              sensorOwner       - sensor owner id
 *                     sensorNumber      - sensor id
 *                     intf              - ipmi interface
 * Output:             sensorReadingData - ipmi response structure
 * Return:             1 on error
 *                     0 if successful
 */
static
int oem_dell_get_sensor_reading(struct ipmi_intf *intf, unsigned char sensornumber,
				sensorreadingtype *psensorreadingdata)
{
	struct ipmi_rq req;
	struct ipmi_rs *rsp;
	int rc = 0;
	memset(&req, 0, sizeof(req));
	req.msg.netfn = IPMI_NETFN_SE;
	req.msg.lun = 0;
	req.msg.cmd = GET_SENSOR_READING;
	req.msg.data = &sensornumber;
	req.msg.data_len = 1;
	if( NULL == psensorreadingdata ) {
		return -1;
	}
	memset(psensorreadingdata, 0, sizeof(sensorreadingtype));
	rsp = intf->sendrecv(intf, &req);
	if(NULL == rsp) {
		return 1;
	} else if( rsp->ccode ) {
		return 1;
	}
	memcpy(psensorreadingdata, rsp->data, sizeof(sensorreadingtype));
	/* if there is an error transmitting ipmi command, return error */
	if( rsp->ccode ) {
		rc = 1;
	}
	/* if sensor messages are disabled, return error*/
	if((!(rsp->data[1]& 0xC0)) || ((rsp->data[1] & 0x20))) {
		rc = 1;
	}
	return rc;
}

/*
 * Function Name:   oem_dell_get_power_capstatus_command
 *
 * Description:     This function gets the power cap status
 * Input:           intf                 - ipmi interface
 * Global:          PowercapSetable_flag - power cap status
 * Output:
 *
 * Return:
 */
static
int oem_dell_get_power_capstatus_command(struct ipmi_intf *intf)
{
	struct ipmi_rs *rsp = NULL;
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
	if(NULL == rsp) {
		lprintf(LOG_ERR, 
				"Error getting powercap status");
		return -1;
	} else if(( iDRAC_FLAG_12_13 ) && (rsp->ccode == LICENSE_NOT_SUPPORTED)) {
		lprintf(LOG_ERR, 
				"FM001 : A required license is missing or expired");
		return -1; /* Return Error as unlicensed */
	} else if( rsp->ccode ) {
		lprintf(LOG_ERR, 
				"Error getting powercap statusr: %s", 
				val2str(rsp->ccode, completion_code_vals));
		return -1;
	}
	if(rsp->data[0] & 0x02) {
		powercapsetable_flag = 1;
	}
	if(rsp->data[0] & 0x01) {
		powercapstatusflag = 1;
	}
	return 0;
}

/*
 * Function Name:    oem_dell_set_power_capstatus_command
 *
 * Description:      This function sets the power cap status
 * Input:            intf     - ipmi interface
 *                   val      - power cap status
 * Output:
 *
 * Return:
 */
static
int oem_dell_set_power_capstatus_command(struct ipmi_intf *intf, uint8_t val)
{
	struct ipmi_rs *rsp = NULL;
	struct ipmi_rq req = {0};
	uint8_t data[2];
	uint8_t input_length = 0;
	if(oem_dell_get_power_capstatus_command(intf) < 0) {
		return -1;
	}
	if(powercapsetable_flag != 1) {
		lprintf(LOG_ERR, 
				"Can not set powercap on this system");
		return -1;
	}
	req.msg.netfn = DELL_OEM_NETFN;
	req.msg.lun = 0;
	req.msg.cmd = IPMI_DELL_POWER_CAP_STATUS;
	req.msg.data_len = 2;
	req.msg.data = data;
	data[input_length++] = 0;
	data[input_length++] = val;
	rsp = intf->sendrecv(intf, &req);
	if(NULL == rsp) {
		lprintf(LOG_ERR, 
				"Error setting powercap status");
		return -1;
	} else if((iDRAC_FLAG_12_13) && (rsp->ccode == LICENSE_NOT_SUPPORTED)) {
		lprintf(LOG_ERR, 
				"FM001 : A required license is missing or expired");
		return -1; /* return unlicensed Error code */
	} else if( rsp->ccode ) {
		lprintf(LOG_ERR, 
				"Error setting powercap statusr: %s", 
				val2str(rsp->ccode, completion_code_vals));
		return -1;
	}
	return 0;
}

/*
 * Function Name:    oem_dell_powermgmt
 *
 * Description:      This function print the powermonitor details
 * Input:            intf     - ipmi interface
 * Output:
 *
 * Return:
 */
static
int oem_dell_powermgmt(struct ipmi_intf *intf)
{
#define MAX_TIME_LEN 26
	struct ipmi_rs *rsp = NULL;
	struct ipmi_rq req = {0};
	uint8_t msg_data[2];
	uint8_t input_length = 0;
	uint32_t cumstarttimeconv = 0;
	uint32_t cumreadingconv = 0;
	uint32_t maxpeakstarttimeconv = 0;
	uint32_t amppeaktimeconv = 0;
	uint16_t ampreadingconv = 0;
	uint32_t wattpeaktimeconv = 0;
	uint32_t wattreadingconv = 0;
	uint32_t bmctimeconv = 0;
	uint32_t *bmctimeconvval = 0;

	ipmi_power_monitor *pwrmonitorinfo;

	char cumstarttime[MAX_TIME_LEN] = {0};
	char maxpeakstarttime[MAX_TIME_LEN] = {0};
	char amppeaktime[MAX_TIME_LEN] = {0};
	char wattpeaktime[MAX_TIME_LEN] = {0};
	char bmctime[MAX_TIME_LEN] = {0};

	int ampreading;
	int ampreadingremainder;
	int remainder;
	int wattreading;

	memset(&req, 0, sizeof(req));
	req.msg.netfn = IPMI_NETFN_STORAGE;
	req.msg.lun = 0;
	req.msg.cmd = IPMI_CMD_GET_SEL_TIME;

	rsp = intf->sendrecv(intf, &req);
	if(NULL == rsp) {
		lprintf(LOG_ERR, 
				"Error getting BMC time info.");
		return -1;
	}
	if( rsp->ccode ) {
		lprintf(LOG_ERR, 
				"Error getting power management information, return code %x", 
				rsp->ccode);
		return -1;
	}
	bmctimeconvval = (uint32_t*)rsp->data;
# if WORDS_BIGENDIAN
	bmctimeconv = BSWAP_32(*bmctimeconvval);
# else
	bmctimeconv = *bmctimeconvval;
# endif

	/* get powermanagement info*/
	req.msg.netfn = DELL_OEM_NETFN;
	req.msg.lun = 0x0;
	req.msg.cmd = GET_PWRMGMT_INFO_CMD;
	req.msg.data = msg_data;
	req.msg.data_len = 2;

	memset(msg_data, 0, 2);
	msg_data[input_length++] = 0x07;
	msg_data[input_length++] = 0x01;

	rsp = intf->sendrecv(intf, &req);
	if(NULL == rsp) {
		lprintf(LOG_ERR, 
				"Error getting power management information.");
		return -1;
	}

	if((iDRAC_FLAG_12_13) && (rsp->ccode == LICENSE_NOT_SUPPORTED)) {
		lprintf(LOG_ERR, 
				"FM001 : A required license is missing or expired");
		return -1;
	} else if((rsp->ccode == IPMI_CC_INV_CMD)||(rsp->ccode == IPMI_CC_REQ_DATA_NOT_PRESENT)) {
		lprintf(LOG_ERR, 
				"Error getting power management information: "
				"Command not supported on this system.");
		return -1;
	}else if( rsp->ccode ) {
		lprintf(LOG_ERR, 
				"Error getting power management information, return code %x", 
				rsp->ccode);
		return -1;
	}

	pwrmonitorinfo = (ipmi_power_monitor*)rsp->data;
# if WORDS_BIGENDIAN
	cumstarttimeconv = BSWAP_32(pwrmonitorinfo->cumstarttime);
	cumreadingconv = BSWAP_32(pwrmonitorinfo->cumreading);
	maxpeakstarttimeconv = BSWAP_32(pwrmonitorinfo->maxpeakstarttime);
	amppeaktimeconv = BSWAP_32(pwrmonitorinfo->amppeaktime);
	ampreadingconv = BSWAP_16(pwrmonitorinfo->ampreading);
	wattpeaktimeconv = BSWAP_32(pwrmonitorinfo->wattpeaktime);
	wattreadingconv = BSWAP_16(pwrMonitorInfo->wattReading);
# else
	cumstarttimeconv = pwrmonitorinfo->cumstarttime;
	cumreadingconv = pwrmonitorinfo->cumreading;
	maxpeakstarttimeconv = pwrmonitorinfo->maxpeakstarttime;
	amppeaktimeconv = pwrmonitorinfo->amppeaktime;
	ampreadingconv = pwrmonitorinfo->ampreading;
	wattpeaktimeconv = pwrmonitorinfo->wattpeaktime;
	wattreadingconv = pwrmonitorinfo->wattreading;
# endif

	oem_dell_time_to_str(cumstarttimeconv, cumstarttime);
	oem_dell_time_to_str(maxpeakstarttimeconv, maxpeakstarttime);
	oem_dell_time_to_str(amppeaktimeconv, amppeaktime);
	oem_dell_time_to_str(wattpeaktimeconv, wattpeaktime);
	oem_dell_time_to_str(bmctimeconv, bmctime);

	remainder = (cumreadingconv % 1000);
	cumreadingconv = cumreadingconv / 1000;
	remainder = (remainder + 50) / 100;

	ampreading = ampreadingconv;
	ampreadingremainder = ampreading%10;
	ampreading = ampreading/10;

	wattreading = wattreadingconv;

	printf("Power Tracking Statistics\n");
	printf("Statistic      : Cumulative Energy Consumption\n");
	printf("Start Time     : %s", cumstarttime);
	printf("Finish Time    : %s", bmctime);
	printf("Reading        : %d.%d kWh\n\n", cumreadingconv, remainder);

	printf("Statistic      : System Peak Power\n");
	printf("Start Time     : %s", maxpeakstarttime);
	printf("Peak Time      : %s", wattpeaktime);
	printf("Peak Reading   : %d W\n\n", wattreading);

	printf("Statistic      : System Peak Amperage\n");
	printf("Start Time     : %s", maxpeakstarttime);
	printf("Peak Time      : %s", amppeaktime);
	printf("Peak Reading   : %d.%d A\n", ampreading, ampreadingremainder);
	return 0;
}

/*
 * Function Name:    oem_dell_powermgmt_clear
 *
 * Description:     This function clears peakpower / cumulativepower value
 * Input:           intf           - ipmi interface
 *                  clearValue     - peakpower / cumulativepower
 * Output:
 *
 * Return:
 */
static
int oem_dell_powermgmt_clear(struct ipmi_intf *intf, uint8_t clearvalue)
{
	struct ipmi_rs *rsp = NULL;
	struct ipmi_rq req = {0};
	uint8_t cleartype = 1;
	uint8_t msg_data[3];
	uint8_t input_length = 0;
	if(clearvalue) {
		cleartype = 2;
	}
	/* clear powermanagement info*/
	req.msg.netfn = DELL_OEM_NETFN;
	req.msg.lun = 0;
	req.msg.cmd = CLEAR_PWRMGMT_INFO_CMD;
	req.msg.data = msg_data;
	req.msg.data_len = 3;
	memset(msg_data, 0, 3);
	msg_data[input_length++] = 0x07;
	msg_data[input_length++] = 0x01;
	msg_data[input_length++] = cleartype;

	rsp = intf->sendrecv(intf, &req);
	if(NULL == rsp) {
		lprintf(LOG_ERR, 
				"Error clearing power values.");
		return -1;
	} else if((iDRAC_FLAG_12_13)
			&&(rsp->ccode == LICENSE_NOT_SUPPORTED)) {
		lprintf(LOG_ERR, 
				"FM001 : A required license is missing or expired");
		return -1;
	} else if(rsp->ccode == IPMI_CC_INV_CMD) {
		lprintf(LOG_ERR, 
				"Error clearing power values, command not supported on this system.");
		return -1;
	} else if( rsp->ccode ) {
		lprintf(LOG_ERR, 
				"Error clearing power values: %s", 
				val2str(rsp->ccode, completion_code_vals));
		return -1;
	}
	return 0;
}

/*
 * Function Name:    oem_dell_watt_to_btuphr_conversion
 *
 * Description:      This function converts the power value in watt to btuphr
 * Input:            powerinwatt     - power in watt
 *
 * Output:           power in btuphr
 *
 * Return:
 */
static
uint64_t oem_dell_watt_to_btuphr_conversion(uint32_t powerinwatt)
{
	uint64_t powerinbtuphr;
	powerinbtuphr = (3.413 * powerinwatt);
	return(powerinbtuphr);
}

/*
 * Function Name:    oem_dell_btuphr_to_watt_conversion
 *
 * Description:      This function converts the power value in  btuphr to watt
 * Input:            powerinbtuphr   - power in btuphr
 *
 * Output:           power in watt
 *
 * Return:
 */
static
uint32_t oem_dell_btuphr_to_watt_conversion(uint64_t powerinbtuphr)
{
	uint32_t powerinwatt;
	/*returning the floor value*/
	powerinwatt = (powerinbtuphr / 3.413);
	return(powerinwatt);
}

/*
 * Function Name:        oem_dell_get_power_headroom_command
 *
 * Description:          This function prints the Power consumption information
 * Input:                intf    - ipmi interface
 *                       unit    - watt / btuphr
 * Output:
 *
 * Return:
 */
static
int oem_dell_get_power_headroom_command(struct ipmi_intf *intf, uint8_t unit)
{
	struct ipmi_rs *rsp = NULL;
	struct ipmi_rq req = {0};
	uint64_t peakpowerheadroombtuphr = 0;
	uint64_t instantpowerhearoom = 0;

	req.msg.netfn = DELL_OEM_NETFN;
	req.msg.lun = 0;
	req.msg.cmd = GET_PWR_HEADROOM_CMD;
	req.msg.data_len = 0;

	rsp = intf->sendrecv(intf, &req);
	if(NULL == rsp) {
		lprintf(LOG_ERR, 
				"Error getting power headroom status");
		return -1;
	} else if((iDRAC_FLAG_12_13)
			&&(rsp->ccode == LICENSE_NOT_SUPPORTED)) {
		lprintf(LOG_ERR, 
				"FM001 : A required license is missing or expired");
		return -1;
	} else if((rsp->ccode == IPMI_CC_INV_CMD) || (rsp->ccode == IPMI_CC_REQ_DATA_NOT_PRESENT)) {
		lprintf(LOG_ERR, 
				"Error getting power headroom status: "
				"Command not supported on this system ");
		return -1;
	} else if( rsp->ccode ) {
		lprintf(LOG_ERR, 
				"Error getting power headroom status: %s", 
				val2str(rsp->ccode, completion_code_vals));
		return -1;
	}
	if(verbose > 1) {
		/* need to look into */
		printf("power headroom  Data               : %x %x %x %x ", rsp->data[0], 
				rsp->data[1], rsp->data[2], rsp->data[3]);
	}
	powerheadroom = *(( power_headroom *)rsp->data);
# if WORDS_BIGENDIAN
	powerheadroom.instheadroom = BSWAP_16(powerheadroom.instheadroom);
	powerheadroom.peakheadroom = BSWAP_16(powerheadroom.peakheadroom);
# endif
	printf("Headroom\n");
	printf("Statistic                     Reading\n");
	if(unit == btuphr) {
		peakpowerheadroombtuphr = oem_dell_watt_to_btuphr_conversion(powerheadroom.peakheadroom);
		instantpowerhearoom = oem_dell_watt_to_btuphr_conversion(powerheadroom.instheadroom);
		printf("System Instantaneous Headroom : %" PRId64 " BTU/hr\n", 
				instantpowerhearoom);
		printf("System Peak Headroom          : %" PRId64 " BTU/hr\n", 
				peakpowerheadroombtuphr);
	} else{
		printf("System Instantaneous Headroom : %d W\n", 
				powerheadroom.instheadroom);
		printf("System Peak Headroom          : %d W\n", 
				powerheadroom.peakheadroom);
	}
	return 0;
}

/*
 * Function Name:       oem_dell_get_power_consumption_data
 *
 * Description:         This function updates the instant Power consumption information
 * Input:               intf - ipmi interface
 * Output:              power consumption current reading
 *                      Assumption value will be in Watt.
 *
 * Return:
 */
static
int oem_dell_get_power_consumption_data(struct ipmi_intf *intf, uint8_t unit)
{
	sensorreadingtype sensorreadingdata;

	struct ipmi_rs *rsp = NULL;
	struct sdr_record_list *sdr = NULL;
	int readingbtuphr = 0;
	int warning_threshbtuphr = 0;
	int failure_threshbtuphr = 0;
	int status = 0;
	int sensor_number = 0;
	sdr = ipmi_sdr_find_sdr_byid(intf, "System Level");
	if( NULL == sdr ) {
		lprintf(LOG_ERR, 
				"Error : Can not access the System Level sensor data");
		return -1;
	}
	sensor_number = sdr->record.common->keys.sensor_num;
	oem_dell_get_sensor_reading(intf, sensor_number, &sensorreadingdata);
	rsp = ipmi_sdr_get_sensor_thresholds(intf, 
			sdr->record.common->keys.sensor_num, 
			sdr->record.common->keys.owner_id, 
			sdr->record.common->keys.lun, 
			sdr->record.common->keys.channel);
	if(NULL == rsp ||  rsp->ccode ) {
		lprintf(LOG_ERR, 
				"Error : Can not access the System Level sensor data");
		return -1;
	}
	readingbtuphr = sdr_convert_sensor_reading(sdr->record.full, 
			sensorreadingdata.sensorreading);
	warning_threshbtuphr = sdr_convert_sensor_reading(sdr->record.full, 
			rsp->data[4]);
	failure_threshbtuphr = sdr_convert_sensor_reading(sdr->record.full, 
			rsp->data[5]);

	printf("System Board System Level\n");
	if(unit == btuphr) {
		readingbtuphr = oem_dell_watt_to_btuphr_conversion(readingbtuphr);
		warning_threshbtuphr = oem_dell_watt_to_btuphr_conversion(warning_threshbtuphr);
		failure_threshbtuphr = oem_dell_watt_to_btuphr_conversion( failure_threshbtuphr);

		printf("Reading                        : %d BTU/hr\n", readingbtuphr);
		printf("Warning threshold      : %d BTU/hr\n", warning_threshbtuphr);
		printf("Failure threshold      : %d BTU/hr\n", failure_threshbtuphr);
	} else{
		printf("Reading                        : %d W \n", readingbtuphr);
		printf("Warning threshold      : %d W \n", (warning_threshbtuphr));
		printf("Failure threshold      : %d W \n", (failure_threshbtuphr));
	}
	return status;
}

/*
 * Function Name:      oem_dell_get_instan_power_consmpt_data
 *
 * Description:        This function updates the instant Power consumption information
 * Input:              intf - ipmi interface
 * Output:             instpowerconsumptiondata - instant Power consumption information
 *
 * Return:
 */
static
int oem_dell_get_instan_power_consmpt_data(struct ipmi_intf *intf, 
					ipmi_inst_power_consumption_data *instpowerconsumptiondata)
{
	struct ipmi_rs *rsp;
	struct ipmi_rq req = {0};
	uint8_t msg_data[2];
	uint8_t input_length = 0;
	/*get instantaneous power consumption command*/
	req.msg.netfn = DELL_OEM_NETFN;
	req.msg.lun = 0;
	req.msg.cmd = GET_PWR_CONSUMPTION_CMD;
	req.msg.data = msg_data;
	req.msg.data_len = 2;
	memset(msg_data, 0, 2);
	msg_data[input_length++] = 0x0A;
	msg_data[input_length++] = 0x00;

	rsp = intf->sendrecv(intf, &req);
	if(NULL == rsp) {
		lprintf(LOG_ERR, 
				"Error getting instantaneous power consumption data .");
		return -1;
	} else if((iDRAC_FLAG_12_13)
			&&(rsp->ccode == LICENSE_NOT_SUPPORTED)) {
		lprintf(LOG_ERR, 
				"FM001 : A required license is missing or expired");
		return -1;
	} else if((rsp->ccode == IPMI_CC_INV_CMD) || (rsp->ccode == IPMI_CC_REQ_DATA_NOT_PRESENT)) {
		lprintf(LOG_ERR, 
				"Error getting instantaneous power consumption data: "
				"Command not supported on this system.");
		return -1;
	} else if( rsp->ccode ) {
		lprintf(LOG_ERR, 
				"Error getting instantaneous power consumption data: %s", 
				val2str(rsp->ccode, completion_code_vals));
		return -1;
	}
	*instpowerconsumptiondata = *((ipmi_inst_power_consumption_data *)(rsp->data));
#if WORDS_BIGENDIAN
	instpowerconsumptiondata->instanpowerconsumption = BSWAP_16(instpowerconsumptiondata->instanpowerconsumption);
	instpowerconsumptiondata->instanApms = BSWAP_16(instpowerconsumptiondata->instanApms);
	instpowerconsumptiondata->resv1 = BSWAP_16(instpowerconsumptiondata->resv1);
#endif
	return 0;
}

/*
 * Function Name:      oem_dell_print_get_instan_power_Amps_data
 *
 * Description:        This function prints the instant Power consumption information
 * Input:              instpowerconsumptiondata - instant Power consumption information
 * Output:
 *
 * Return:
 */
static
void oem_dell_print_get_instan_power_amps_data(ipmi_inst_power_consumption_data instpowerconsumptiondata)
{
	uint16_t intampsval = 0;
	uint16_t decimalampsval = 0;
	if(instpowerconsumptiondata.instanApms > 0) {
		decimalampsval = (instpowerconsumptiondata.instanApms % 10);
		intampsval = instpowerconsumptiondata.instanApms / 10;
	}
	printf("\nAmperage value: %d.%d A \n", intampsval, decimalampsval);
}

/*
 * Function Name:     oem_dell_print_get_power_consmpt_data
 *
 * Description:       This function prints the Power consumption information
 * Input:             intf            - ipmi interface
 *                    unit            - watt / btuphr
 * Output:
 *
 * Return:
 */
static
int oem_dell_print_get_power_consmpt_data(struct ipmi_intf *intf, uint8_t unit)
{
	int rc = 0;
	ipmi_inst_power_consumption_data instpowerconsumptiondata = {0};
	printf("\nPower consumption information\n");
	rc = oem_dell_get_power_consumption_data(intf, unit);
	if(rc == (-1)) {
		return rc;
	}
	rc = oem_dell_get_instan_power_consmpt_data(intf, &instpowerconsumptiondata);
	if(rc == (-1)) {
		return rc;
	}
	oem_dell_print_get_instan_power_amps_data(instpowerconsumptiondata);
	rc = oem_dell_get_power_headroom_command(intf, unit);
	if(rc == (-1)) {
		return rc;
	}
	return rc;
}

/*
 * Function Name:   oem_dell_get_avgpower_consmpt_history
 *
 * Description:     This function updates the average power consumption information
 * Input:           intf            - ipmi interface
 * Output:          pavgpower- average power consumption information
 *
 * Return:
 */
static
int oem_dell_get_avgpower_consmpt_history(struct ipmi_intf *intf, 
					ipmi_avgpower_consump_history *pavgpower)
{
	int rc;
	uint8_t *rdata;
	rc = ipmi_mc_getsysinfo(intf, 0xeb, 0, 0, sizeof(*pavgpower), pavgpower);
	if(rc < 0) {
		lprintf(LOG_ERR, 
				"Error getting average power consumption history data.");
		return -1;
	} else if((iDRAC_FLAG_12_13) && (rc == LICENSE_NOT_SUPPORTED)) {
		lprintf(LOG_ERR, 
				"FM001 : A required license is missing or expired");
		return -1;
	} else if((rc == IPMI_CC_INV_CMD) || (rc == IPMI_CC_REQ_DATA_NOT_PRESENT)) {
		lprintf(LOG_ERR, 
				"Error getting average power consumption history data: "
				"Command not supported on this system.");
		return -1;
	} else if(rc != 0) {
		lprintf(LOG_ERR, 
				"Error getting average power consumption history data: %s", 
				val2str(rc, completion_code_vals));
		return -1;
	}
	if(verbose > 1) {
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
 * Function Name:    oem_dell_get_peakpower_consmpt_history
 *
 * Description:      This function updates the peak power consumption information
 * Input:            intf            - ipmi interface
 * Output:           pavgpower- peak power consumption information
 *
 * Return:
 */
static
int oem_dell_get_peakpower_consmpt_history(struct ipmi_intf *intf, 
					ipmi_power_consump_history *pstpeakpower)
{
	uint8_t *rdata;
	int rc;
	rc = ipmi_mc_getsysinfo(intf, 0xec, 0, 0, sizeof(*pstpeakpower), 
			pstpeakpower);
	if(rc < 0) {
		lprintf(LOG_ERR, 
				"Error getting  peak power consumption history data.");
		return -1;
	} else if((iDRAC_FLAG_12_13) && (rc == LICENSE_NOT_SUPPORTED)) {
		lprintf(LOG_ERR, 
				"FM001 : A required license is missing or expired");
		return -1;
	} else if((rc == IPMI_CC_INV_CMD) || (rc == IPMI_CC_REQ_DATA_NOT_PRESENT)) {
		lprintf(LOG_ERR, 
				"Error getting peak power consumption history data: "
				"Command not supported on this system.");
		return -1;
	} else if( rc ) {
		lprintf(LOG_ERR, 
				"Error getting peak power consumption history data: %s", 
				val2str(rc, completion_code_vals));
		return -1;
	}
	if(verbose > 1) {
		rdata = (void *)pstpeakpower;
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
	pstpeakpower->lastminutepower = BSWAP_16(pstpeakpower->lastminutepower);
	pstpeakpower->lasthourpower = BSWAP_16(pstpeakpower->lasthourpower);
	pstpeakpower->lastdaypower = BSWAP_16(pstpeakpower->lastdaypower);
	pstpeakpower->lastweakpower = BSWAP_16(pstpeakpower->lastweakpower);
	pstpeakpower->lastminutepowertime = BSWAP_32(pstpeakpower->lastminutepowertime);
	pstpeakpower->lasthourpowertime = BSWAP_32(pstpeakpower->lasthourpowertime);
	pstpeakpower->lastdaypowertime = BSWAP_32(pstpeakpower->lastdaypowertime);
	pstpeakpower->lastweekpowertime = BSWAP_32(pstpeakpower->lastweekpowertime);
#endif
	return 0;
}

/*
 * Function Name:    oem_dell_get_minpower_consmpt_history
 *
 * Description:      This function updates the peak power consumption information
 * Input:            intf            - ipmi interface
 * Output:           pavgpower- peak power consumption information
 *
 * Return:
 */
static
int oem_dell_get_minpower_consmpt_history(struct ipmi_intf *intf, 
					ipmi_power_consump_history *pstminpower)
{
	uint8_t *rdata;
	int rc;
	rc = ipmi_mc_getsysinfo(intf, 0xed, 0, 0, sizeof(*pstminpower), 
			pstminpower);
	if(rc < 0) {
		lprintf(LOG_ERR, 
				"Error getting  peak power consumption history data .");
		return -1;
	} else if((iDRAC_FLAG_12_13) && (rc == LICENSE_NOT_SUPPORTED)) {
		lprintf(LOG_ERR, 
				"FM001 : A required license is missing or expired");
		return -1;
	} else if((rc == IPMI_CC_INV_CMD) ||(rc == IPMI_CC_REQ_DATA_NOT_PRESENT)) {
		lprintf(LOG_ERR, 
				"Error getting peak power consumption history data: "
				"Command not supported on this system.");
		return -1;
	} else if( rc ) {
		lprintf(LOG_ERR, 
				"Error getting peak power consumption history data: %s", 
				val2str(rc, completion_code_vals));
		return -1;
	}
	if(verbose > 1) {
		rdata = (void *)pstminpower;
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
	pstminpower->lastminutepower = BSWAP_16(pstminpower->lastminutepower);
	pstminpower->lasthourpower = BSWAP_16(pstminpower->lasthourpower);
	pstminpower->lastdaypower = BSWAP_16(pstminpower->lastdaypower);
	pstminpower->lastweakpower = BSWAP_16(pstminpower->lastweakpower);
	pstminpower->lastminutepowertime = BSWAP_32(pstminpower->lastminutepowertime);
	pstminpower->lasthourpowertime = BSWAP_32(pstminpower->lasthourpowertime);
	pstminpower->lastdaypowertime = BSWAP_32(pstminpower->lastdaypowertime);
	pstminpower->lastweekpowertime = BSWAP_32(pstminpower->lastweekpowertime);
# endif
	return 0;
}

/*
 * Function Name:    oem_dell_print_power_consmpt_history
 *
 * Description:      This function print the average and peak power consumption information
 * Input:            intf      - ipmi interface
 *                   unit      - watt / btuphr
 * Output:
 *
 * Return:
 */
static
int oem_dell_print_power_consmpt_history(struct ipmi_intf *intf, int unit)
{
	char timestr[30];
	uint32_t lastminutepeakpower;
	uint32_t lasthourpeakpower;
	uint32_t lastdaypeakpower;
	uint32_t lastweekpeakpower;
	uint64_t tempbtuphrconv;
	int rc = 0;

	ipmi_avgpower_consump_history avgpower;
	ipmi_power_consump_history stminpower;
	ipmi_power_consump_history stpeakpower;

	rc = oem_dell_get_avgpower_consmpt_history(intf, &avgpower);
	if(rc == -1) {
		return rc;
	}

	rc = oem_dell_get_peakpower_consmpt_history(intf, &stpeakpower);
	if(rc == -1) {
		return rc;
	}

	rc = oem_dell_get_minpower_consmpt_history(intf, &stminpower);
	if(rc == -1) {
		return rc;
	}
	if(rc != 0) {
		return rc;
	}
	printf("Power Consumption History\n\n");
	/* The fields are alligned manually changing the spaces will alter
	 * the alignment*/
	printf("Statistic                   Last Minute     Last Hour     "
			"Last Day     Last Week\n\n");
	if(unit == btuphr) {
		printf("Average Power Consumption  ");
		tempbtuphrconv = oem_dell_watt_to_btuphr_conversion(avgpower.lastminutepower);
		printf("%4" PRId64 " BTU/hr     ", tempbtuphrconv);
		tempbtuphrconv = oem_dell_watt_to_btuphr_conversion(avgpower.lasthourpower);
		printf("%4" PRId64 " BTU/hr   ", tempbtuphrconv);
		tempbtuphrconv = oem_dell_watt_to_btuphr_conversion(avgpower.lastdaypower);
		printf("%4" PRId64 " BTU/hr  ", tempbtuphrconv);
		tempbtuphrconv = oem_dell_watt_to_btuphr_conversion(avgpower.lastweakpower);
		printf("%4" PRId64 " BTU/hr\n", tempbtuphrconv);

		printf("Max Power Consumption      ");
		tempbtuphrconv = oem_dell_watt_to_btuphr_conversion(stpeakpower.lastminutepower);
		printf("%4" PRId64 " BTU/hr     ", tempbtuphrconv);
		tempbtuphrconv = oem_dell_watt_to_btuphr_conversion(stpeakpower.lasthourpower);
		printf("%4" PRId64 " BTU/hr   ", tempbtuphrconv);
		tempbtuphrconv = oem_dell_watt_to_btuphr_conversion(stpeakpower.lastdaypower);
		printf("%4" PRId64 " BTU/hr  ", tempbtuphrconv);
		tempbtuphrconv = oem_dell_watt_to_btuphr_conversion(stpeakpower.lastweakpower);
		printf("%4" PRId64 " BTU/hr\n", tempbtuphrconv);

		printf("Min Power Consumption      ");
		tempbtuphrconv = oem_dell_watt_to_btuphr_conversion(stminpower.lastminutepower);
		printf("%4" PRId64 " BTU/hr     ", tempbtuphrconv);
		tempbtuphrconv = oem_dell_watt_to_btuphr_conversion(stminpower.lasthourpower);
		printf("%4" PRId64 " BTU/hr   ", tempbtuphrconv);
		tempbtuphrconv = oem_dell_watt_to_btuphr_conversion(stminpower.lastdaypower);
		printf("%4" PRId64 " BTU/hr  ", tempbtuphrconv);
		tempbtuphrconv = oem_dell_watt_to_btuphr_conversion(stminpower.lastweakpower);
		printf("%4" PRId64 " BTU/hr\n\n", tempbtuphrconv);
	} else{
		printf("Average Power Consumption  ");
		tempbtuphrconv = (avgpower.lastminutepower);
		printf("%4" PRId64 " W          ", tempbtuphrconv);
		tempbtuphrconv = (avgpower.lasthourpower);
		printf("%4" PRId64 " W        ", tempbtuphrconv);
		tempbtuphrconv = (avgpower.lastdaypower);
		printf("%4" PRId64 " W       ", tempbtuphrconv);
		tempbtuphrconv = (avgpower.lastweakpower);
		printf("%4" PRId64 " W   \n", tempbtuphrconv);

		printf("Max Power Consumption      ");
		tempbtuphrconv = (stpeakpower.lastminutepower);
		printf("%4" PRId64 " W          ", tempbtuphrconv);
		tempbtuphrconv = (stpeakpower.lasthourpower);
		printf("%4" PRId64 " W        ", tempbtuphrconv);
		tempbtuphrconv = (stpeakpower.lastdaypower);
		printf("%4" PRId64 " W       ", tempbtuphrconv);
		tempbtuphrconv = (stpeakpower.lastweakpower);
		printf("%4" PRId64 " W   \n", tempbtuphrconv);

		printf("Min Power Consumption      ");
		tempbtuphrconv = (stminpower.lastminutepower);
		printf("%4" PRId64 " W          ", tempbtuphrconv);
		tempbtuphrconv = (stminpower.lasthourpower);
		printf("%4" PRId64 " W        ", tempbtuphrconv);
		tempbtuphrconv = (stminpower.lastdaypower);
		printf("%4" PRId64 " W       ", tempbtuphrconv);
		tempbtuphrconv = (stminpower.lastweakpower);
		printf("%4" PRId64 " W   \n\n", tempbtuphrconv);
	}

	lastminutepeakpower = stpeakpower.lastminutepowertime;
	lasthourpeakpower = stpeakpower.lasthourpowertime;
	lastdaypeakpower = stpeakpower.lastdaypowertime;
	lastweekpeakpower = stpeakpower.lastweekpowertime;

	printf("Max Power Time\n");
	oem_dell_time_to_str(lastminutepeakpower, timestr);
	printf("Last Minute     : %s", timestr);
	oem_dell_time_to_str(lasthourpeakpower, timestr);
	printf("Last Hour       : %s", timestr);
	oem_dell_time_to_str(lastdaypeakpower, timestr);
	printf("Last Day        : %s", timestr);
	oem_dell_time_to_str(lastweekpeakpower, timestr);
	printf("Last Week       : %s", timestr);

	lastminutepeakpower = stminpower.lastminutepowertime;
	lasthourpeakpower = stminpower.lasthourpowertime;
	lastdaypeakpower = stminpower.lastdaypowertime;
	lastweekpeakpower = stminpower.lastweekpowertime;

	printf("Min Power Time\n");
	oem_dell_time_to_str(lastminutepeakpower, timestr);
	printf("Last Minute     : %s", timestr);
	oem_dell_time_to_str(lasthourpeakpower, timestr);
	printf("Last Hour       : %s", timestr);
	oem_dell_time_to_str(lastdaypeakpower, timestr);
	printf("Last Day        : %s", timestr);
	oem_dell_time_to_str(lastweekpeakpower, timestr);
	printf("Last Week       : %s", timestr);
	return rc;
}

/*
 * Function Name:    oem_dell_get_power_cap
 *
 * Description:      This function updates the power cap information
 * Input:            intf         - ipmi interface
 * Output:           ipmipowercap - power cap information
 *
 * Return:
 */
static
int oem_dell_get_power_cap(struct ipmi_intf *intf, ipmi_power_cap *ipmipowercap)
{
	uint8_t *rdata;
	int rc;
	rc = ipmi_mc_getsysinfo(intf, IPMI_DELL_POWER_CAP, 0, 0, 
			sizeof(*ipmipowercap), ipmipowercap);
	if(rc < 0) {
		lprintf(LOG_ERR, 
				"Error getting power cap.");
		return -1;
	} else if((iDRAC_FLAG_12_13) && (rc == LICENSE_NOT_SUPPORTED)) {
		lprintf(LOG_ERR, 
				"FM001 : A required license is missing or expired");
		return -1;
	} else if((rc == IPMI_CC_INV_CMD) || (rc == IPMI_CC_REQ_DATA_NOT_PRESENT)) {
		lprintf(LOG_ERR, 
				"Error getting power cap: "
				"Command not supported on this system.");
		return -1;
	} else if( rc ) {
		lprintf(LOG_ERR, 
				"Error getting power cap: %s", 
				val2str(rc, completion_code_vals));
		return -1;
	}
	if(verbose > 1) {
		rdata = (void*)ipmipowercap;
		printf("power cap  Data               :%x %x %x %x %x %x %x %x %x %x ", 
				rdata[1], rdata[2], rdata[3], 
				rdata[4], rdata[5], rdata[6], rdata[7], 
				rdata[8], rdata[9], rdata[10], rdata[11]);
	}
# if WORDS_BIGENDIAN
	ipmipowercap->powercap = BSWAP_16(ipmipowercap->powercap);
	ipmipowercap->maximumpowerconsmp = BSWAP_16(ipmipowercap->maximumpowerconsmp);
	ipmipowercap->minimumpowerconsmp = BSWAP_16(ipmipowercap->minimumpowerconsmp);
	ipmipowercap->totalnumpowersupp = BSWAP_16(ipmipowercap->totalnumpowersupp);
	ipmipowercap->availablepower = BSWAP_16(ipmipowercap->availablepower);
	ipmipowercap->systemthrottling = BSWAP_16(ipmipowercap->systemthrottling);
	ipmipowercap->resv = BSWAP_16(ipmipowercap->resv);
# endif
	return 0;
}

/*
 * Function Name:    oem_dell_print_power_cap
 *
 * Description:      This function print the power cap information
 * Input:            intf            - ipmi interface
 *                   unit            - watt / btuphr
 * Output:
 * Return:
 */
static
int oem_dell_print_power_cap(struct ipmi_intf *intf, uint8_t unit)
{
	uint64_t tempbtuphrconv;
	int rc;
	ipmi_power_cap ipmipowercap;
	memset(&ipmipowercap, 0, sizeof(ipmipowercap));
	rc = oem_dell_get_power_cap(intf, &ipmipowercap);
	if(rc == 0) {
		if(unit == btuphr) {
			tempbtuphrconv = oem_dell_watt_to_btuphr_conversion(ipmipowercap.maximumpowerconsmp);
			printf("Maximum power: %" PRId64 "  BTU/hr\n", tempbtuphrconv);
			tempbtuphrconv = oem_dell_watt_to_btuphr_conversion(ipmipowercap.minimumpowerconsmp);
			printf("Minimum power: %" PRId64 "  BTU/hr\n", tempbtuphrconv);
			tempbtuphrconv = oem_dell_watt_to_btuphr_conversion(ipmipowercap.powercap);
			printf("Power cap    : %" PRId64 "  BTU/hr\n", tempbtuphrconv);
		} else{
			printf("Maximum power: %d Watt\n", ipmipowercap.maximumpowerconsmp);
			printf("Minimum power: %d Watt\n", ipmipowercap.minimumpowerconsmp);
			printf("Power cap    : %d Watt\n", ipmipowercap.powercap);
		}
	}
	return rc;
}

/*
 * Function Name:     oem_dell_set_power_cap
 *
 * Description:       This function updates the power cap information
 * Input:             intf            - ipmi interface
 *                    unit            - watt / btuphr
 *                    val             - new power cap value
 * Output:
 * Return:
 */
static
int oem_dell_set_power_cap(struct ipmi_intf *intf, int unit, int val)
{
	int rc;
	uint8_t data[13], *rdata = NULL;
	uint8_t input_length = 0;
	uint16_t powercapval = 0;
	uint64_t maxpowerbtuphr = 0;
	uint64_t minpowerbtuphr = 0;
	ipmi_power_cap ipmipowercap;

	if(oem_dell_get_power_capstatus_command(intf) < 0) {
		return -1; /* Adding the failed condition check */
	}
	if(powercapsetable_flag != 1) {
		lprintf(LOG_ERR, 
				"Can not set powercap on this system");
		return -1;
	} else if(powercapstatusflag != 1) {
		lprintf(LOG_ERR, 
				"Power cap set feature is not enabled");
		return -1;
	}
	rc = ipmi_mc_getsysinfo(intf, IPMI_DELL_POWER_CAP, 0, 0, 
			sizeof(ipmipowercap), &ipmipowercap);
	if(rc < 0) {
		lprintf(LOG_ERR, 
				"Error getting power cap.");
		return -1;
	} else if((iDRAC_FLAG_12_13) && (rc == LICENSE_NOT_SUPPORTED)) {
		lprintf(LOG_ERR, 
				"FM001 : A required license is missing or expired");
		return -1;
	} else if(rc == IPMI_CC_INV_CMD) {
		lprintf(LOG_ERR, 
				"Error getting power cap, command not supported on "
				"this system.");
		return -1;
	} else if( rc ) {
		lprintf(LOG_ERR, 
				"Error getting power cap: %s", 
				val2str(rc, completion_code_vals));
		return -1;
	}
	if(verbose > 1) {
		rdata = (void *)&ipmipowercap;
		printf("power cap  Data               :%x %x %x %x %x %x %x %x %x %x %x ", 
				rdata[1], rdata[2], rdata[3], 
				rdata[4], rdata[5], rdata[6], rdata[7], 
				rdata[8], rdata[9], rdata[10], rdata[11]);
	}
# if WORDS_BIGENDIAN
	ipmipowercap.powercap = BSWAP_16(ipmipowercap.powercap);
	ipmipowercap.maximumpowerconsmp = BSWAP_16(ipmipowercap.maximumpowerconsmp);
	ipmipowercap.minimumpowerconsmp = BSWAP_16(ipmipowercap.minimumpowerconsmp);
	ipmipowercap.availablepower = BSWAP_16(ipmipowercap.availablepower);
	ipmipowercap.totalnumpowersupp = BSWAP_16(ipmipowercap.totalnumpowersupp);
# endif
	memset(data, 0, 13);
	data[input_length++] = IPMI_DELL_POWER_CAP;
	powercapval = val;
	data[input_length++] = (powercapval & 0XFF);
	data[input_length++] = ((powercapval & 0XFF00) >> 8);
	data[input_length++] = unit;
	data[input_length++] = ((ipmipowercap.maximumpowerconsmp & 0xFF));
	data[input_length++] = ((ipmipowercap.maximumpowerconsmp & 0xFF00) >> 8);
	data[input_length++] = ((ipmipowercap.minimumpowerconsmp & 0xFF));
	data[input_length++] = ((ipmipowercap.minimumpowerconsmp & 0xFF00) >> 8);
	data[input_length++] = (ipmipowercap.totalnumpowersupp);
	data[input_length++] = ((ipmipowercap.availablepower & 0xFF));
	data[input_length++] = ((ipmipowercap.availablepower & 0xFF00) >> 8);
	data[input_length++] = (ipmipowercap.systemthrottling);
	data[input_length++] = 0x00;

	if(unit == btuphr) {
		val = oem_dell_btuphr_to_watt_conversion(val);
	} else if(unit == percent) {
		if((val < 0) || (val > 100)) {
			lprintf(LOG_ERR, 
					"Cap value is out of boundary conditon it "
					"should be between 0  - 100");
			return -1;
		}
		val = ((val*(ipmipowercap.maximumpowerconsmp
						- ipmipowercap.minimumpowerconsmp)) / 100)
			+ ipmipowercap.minimumpowerconsmp;
		lprintf(LOG_ERR, 
				"Cap value in percentage is  %d ", val);
		input_length = 1;
		data[input_length++] = (val & 0XFF);
		data[input_length++] = ((val & 0XFF00) >> 8);
		data[input_length++] = watt;
	}
	if(((val < ipmipowercap.minimumpowerconsmp)
				|| (val > ipmipowercap.maximumpowerconsmp)) && (unit == watt)) {
		lprintf(LOG_ERR, 
				"Cap value is out of boundary conditon it should be between %d  - %d", 
				ipmipowercap.minimumpowerconsmp, ipmipowercap.maximumpowerconsmp);
		return -1;
	} else if(((val < ipmipowercap.minimumpowerconsmp)
				||(val > ipmipowercap.maximumpowerconsmp)) && (unit == btuphr)) {
		minpowerbtuphr = oem_dell_watt_to_btuphr_conversion(ipmipowercap.minimumpowerconsmp);
		maxpowerbtuphr = oem_dell_watt_to_btuphr_conversion(ipmipowercap.maximumpowerconsmp);
		lprintf(LOG_ERR, 
				"Cap value is out of boundary conditon it should be between %d", 
				minpowerbtuphr);
		lprintf(LOG_ERR, 
				" -%d", maxpowerbtuphr);
		return -1;
	}
	rc = ipmi_mc_setsysinfo(intf, 13, data);
	if(rc < 0) {
		lprintf(LOG_ERR, 
				"Error setting power cap");
		return -1;
	} else if((iDRAC_FLAG_12_13) && (rc == LICENSE_NOT_SUPPORTED)) {
		lprintf(LOG_ERR, 
				"FM001 : A required license is missing or expired");
		return -1;
	} else if(rc > 0) {
		lprintf(LOG_ERR, 
				"Error setting power cap: %s", 
				val2str(rc, completion_code_vals));
		return -1;
	}
	if(verbose > 1) {
		printf("CC for setpowercap :%d ", rc);
	}
	return 0;
}

/*
 * Function Name:   oem_dell_powermonitor_usage
 *
 * Description:     This function prints help message for powermonitor command
 * Input:
 * Output:
 *
 * Return:
 */
static
void oem_dell_powermonitor_usage(void)
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
 * Function Name:   oem_dell_vflash_main
 *
 * Description:   This function processes the delloem vflash command
 * Input:   intf    - ipmi interface
 *   argc    - no of arguments
 *   argv    - argument string array
 * Output:
 *
 * Return:   return code   0 - success
 *  -1 - failure
 */
static
int oem_dell_vflash_main(struct ipmi_intf *intf, int argc, char **argv)
{
	int rc = 0;
	current_arg++;
	rc = oem_dell_vflash_process(intf, current_arg, argv);
	return rc;
}

/*
 * Function Name: get_vflash_compcode_str
 *
 * Description: This function maps the vflash completion code
 * to a string
 * Input : vflash completion code and static array of codes vs strings
 * Output: -
 * Return: returns the mapped string
 */
const char* get_vflash_compcode_str(uint8_t vflashcompcode, const struct vflashstr *vs)
{
	static char un_str[32];
	int i;
	for(i = 0; vs[i].str != NULL; i++) {
		if(vs[i].val == vflashcompcode)
			return vs[i].str;
	}
	memset(un_str, 0, 32);
	snprintf(un_str, 32, "Unknown(0x%02X)", vflashcompcode);
	return un_str;
}

/*
 * Function Name: oem_dell_get_sd_card_info
 *
 * Description: This function prints the vflash Extended SD card info
 * Input : ipmi interface
 * Output: prints the sd card extended info
 * Return: 0 - success -1 - failure
 */
static
int oem_dell_get_sd_card_info(struct ipmi_intf *intf)
{
	struct ipmi_rs *rsp;
	struct ipmi_rq req;
	uint8_t msg_data[2];
	uint8_t input_length = 0;
	uint8_t cardstatus = 0x00;
	ipmi_dell_sdcard_info * sdcardinfoblock;

	input_length = 0;
	msg_data[input_length++] = 0x00;
	msg_data[input_length++] = 0x00;
	req.msg.netfn = DELL_OEM_NETFN;
	req.msg.lun = 0;
	req.msg.cmd = IPMI_GET_EXT_SD_CARD_INFO;
	req.msg.data = msg_data;
	req.msg.data_len = input_length;

	rsp = intf->sendrecv(intf, &req);
	if(NULL == rsp) {
		lprintf(LOG_ERR, 
				"Error in getting SD Card Extended Information");
		return -1;
	} else if( rsp->ccode ) {
		lprintf(LOG_ERR, 
				"Error in getting SD Card Extended Information(%s)", 
				val2str(rsp->ccode, completion_code_vals));
		return -1;
	}

	sdcardinfoblock = (ipmi_dell_sdcard_info *)(void *) rsp->data;

	if((iDRAC_FLAG_12_13)
			&&(sdcardinfoblock->vflashcompcode == VFL_NOT_LICENSED)) {
		lprintf(LOG_ERR, 
				"FM001 : A required license is missing or expired");
		return -1;
	} else if(sdcardinfoblock->vflashcompcode != 0x00) {
		lprintf(LOG_ERR, 
				"Error in getting SD Card Extended Information(%s)", 
				get_vflash_compcode_str(sdcardinfoblock->vflashcompcode, 
					vflash_completion_code_vals));
		return -1;
	}

	if(!(sdcardinfoblock->sdcardstatus & 0x04)) {
		lprintf(LOG_ERR, 
				"vflash SD card is unavailable, please insert the card of");
		lprintf(LOG_ERR, 
				"size 256MB or greater");
		return(-1);
	}

	printf("vflash SD Card Properties\n");
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
			((0x00 == cardstatus) ? "OK" :(
				(cardstatus == 0x03) ? "Undefined" :(
					(cardstatus == 0x02) ? "Critical" : "Warning"))));
	printf("Bootable partition : %10d\n", sdcardinfoblock->bootpartion);
	return 0;
}

/*
 * Function Name: oem_dell_vflash_process
 *
 * Description: This function processes the args for vflash subcmd
 * Input : intf - ipmi interface, arg index, argv array
 * Output: prints help or error with help
 * Return: 0 - Success -1 - failure
 */
static
int oem_dell_vflash_process(struct ipmi_intf *intf, int current_arg, char **argv)
{
	int rc;
	if(strcmp(intf->name, "wmi") && strcmp(intf->name, "open")) {
		lprintf(LOG_ERR, 
				"vflash support is enabled only for wmi and open interface.");
		lprintf(LOG_ERR, "Its not enabled for lan and lanplus interface.");
		return -1;
	}

	if( NULL == argv[current_arg] || strcmp(argv[current_arg], "help") == 0) {
		oem_dell_vflash_usage();
		return 0;
	}
	oem_dell_idracvalidator_command(intf);
	if(!strcmp(argv[current_arg], "info")) {
		current_arg++;
		if( NULL == argv[current_arg] ) {
			oem_dell_vflash_usage();
			return -1;
		} else if(strcmp(argv[current_arg], "Card") == 0) {
			current_arg++;
			if(NULL != argv[current_arg]) {
				oem_dell_vflash_usage();
				return -1;
			}
			rc = oem_dell_get_sd_card_info(intf);
			return rc;
		} else{
			/* TBD: many sub commands are present */
			oem_dell_vflash_usage();
			return -1;
		}
	} else{
		/* TBD other vflash subcommands */
		oem_dell_vflash_usage();
		return -1;
	}
}

/*
 * Function Name: oem_dell_vflash_usage
 *
 * Description: This function displays the usage for using vflash
 * Input : void
 * Output: prints help
 * Return: void
 */
static
void oem_dell_vflash_usage(void)
{
	lprintf(LOG_NOTICE, 
			"");
	lprintf(LOG_NOTICE, 
			"   vflash info Card");
	lprintf(LOG_NOTICE, 
			"      Shows Extended SD Card information");
	lprintf(LOG_NOTICE, 
			"");
}

/*
 * Function Name: oem_dell_setled_usage
 *
 * Description:  This function prints help message for setled command
 * Input:
 * Output:
 *
 * Return:
 */
static
void oem_dell_setled_usage(void)
{
	lprintf(LOG_NOTICE, 
			"");
	lprintf(LOG_NOTICE, 
			"   setled <b:d.f> <state..>");
	lprintf(LOG_NOTICE, 
			"      Set backplane LED state");
	lprintf(LOG_NOTICE, 
			"      b:d.f = PCI Bus:Device.Function of drive(lspci format)");
	lprintf(LOG_NOTICE, 
			"      state = present|online|hotspare|identify|rebuilding|");
	lprintf(LOG_NOTICE, 
			"              fault|predict|critical|failed");
	lprintf(LOG_NOTICE, 
			"");
}

static
int oem_dell_issetledsupported(void)
{
	return setledsupported;
}

static
void oem_dell_checksetledsupport(struct ipmi_intf *intf)
{
	struct ipmi_rs *rsp = NULL;
	struct ipmi_rq req = {0};
	uint8_t data[10];
	uint8_t input_length = 0;

	setledsupported = 0;
	req.msg.netfn = DELL_OEM_NETFN;
	req.msg.lun = 0;
	req.msg.cmd = 0xD5; /* Storage */
	req.msg.data_len = 10;
	req.msg.data = data;

	memset(data, 0, sizeof(data));
	data[input_length++] = 0x01; /* get */
	data[input_length++] = 0x00; /* subcmd:get firmware version */
	data[input_length++] = 0x08; /* length lsb */
	data[input_length++] = 0x00; /* length msb */
	data[input_length++] = 0x00; /* offset lsb */
	data[input_length++] = 0x00; /* offset msb */
	data[input_length++] = 0x00; /* bay id */
	data[input_length++] = 0x00;
	data[input_length++] = 0x00;
	data[input_length++] = 0x00;

	rsp = intf->sendrecv(intf, &req);
	if(NULL == rsp ||  rsp->ccode ) {
		return;
	}
	setledsupported = 1;
}

/*
 * Function Name:    oem_dell_getdrivemap
 *
 * Description:      This function returns mapping of BDF to Bay:Slot
 * Input:            intf         - ipmi interface
 *    bdf  - PCI Address of drive
 *    *bay - Returns bay ID
 *    *slot - Returns slot ID
 * Output:
 *
 * Return:
 */
static
int oem_dell_getdrivemap(struct ipmi_intf *intf, int bus, int device, int function, int *bay, 
			int *slot)
{
	struct ipmi_rs *rsp = NULL;
	struct ipmi_rq req = {0};
	uint8_t data[8];
	uint8_t input_length = 0;
	/* Get mapping of BDF to bay:slot */
	req.msg.netfn = DELL_OEM_NETFN;
	req.msg.lun = 0;
	req.msg.cmd = 0xD5;
	req.msg.data_len = 8;
	req.msg.data = data;

	memset(data, 0, sizeof(data));
	data[input_length++] = 0x01; /* get */
	data[input_length++] = 0x07; /* storage map */
	data[input_length++] = 0x06; /* length lsb */
	data[input_length++] = 0x00; /* length msb */
	data[input_length++] = 0x00; /* offset lsb */
	data[input_length++] = 0x00; /* offset msb */
	data[input_length++] = bus; /* bus */
	data[input_length++] = (device << 3) + function; /* devfn */

	rsp = intf->sendrecv(intf, &req);
	if(NULL == rsp) {
		lprintf(LOG_ERR, 
				"Error issuing getdrivemap command.");
		return -1;
	} else if( rsp->ccode ) {
		lprintf(LOG_ERR, 
				"Error issuing getdrivemap command: %s", 
				val2str(rsp->ccode, completion_code_vals));
		return -1;
	}
	*bay = rsp->data[7];
	*slot = rsp->data[8];
	if(*bay == 0xFF || *slot == 0xFF) {
		lprintf(LOG_ERR, 
				"Error could not get drive bay:slot mapping");
		return -1;
	}
	return 0;
}

/*
 * Function Name:    oem_dell_setled_state
 *
 * Description:      This function updates the LED on the backplane
 * Input:            intf         - ipmi interface
 *    bdf  - PCI Address of drive
 *    state - SES Flags state of drive
 * Output:
 *
 * Return:
 */
static
int oem_dell_setled_state(struct ipmi_intf *intf, int bayid, int slotid, int state)
{
	struct ipmi_rs *rsp = NULL;
	struct ipmi_rq req = {0};
	uint8_t data[20];
	uint8_t input_length = 0;
	/* Issue Drive Status Update to bay:slot */
	req.msg.netfn = DELL_OEM_NETFN;
	req.msg.lun = 0;
	req.msg.cmd = 0xD5;
	req.msg.data_len = 20;
	req.msg.data = data;

	memset(data, 0, sizeof(data));
	data[input_length++] = 0x00; /* set */
	data[input_length++] = 0x04; /* set drive status */
	data[input_length++] = 0x0e; /* length lsb */
	data[input_length++] = 0x00; /* length msb */
	data[input_length++] = 0x00; /* offset lsb */
	data[input_length++] = 0x00; /* offset msb */
	data[input_length++] = 0x0e; /* length lsb */
	data[input_length++] = 0x00; /* length msb */
	data[input_length++] = bayid; /* bayid */
	data[input_length++] = slotid; /* slotid */
	data[input_length++] = state & 0xff; /* state LSB */
	data[input_length++] = state >> 8; /* state MSB; */

	rsp = intf->sendrecv(intf, &req);
	if(NULL == rsp) {
		lprintf(LOG_ERR, 
				"Error issuing setled command.");
		return -1;
	} else if( rsp->ccode ) {
		lprintf(LOG_ERR, 
				"Error issuing setled command: %s", 
				val2str(rsp->ccode, completion_code_vals));
		return -1;
	}
	return 0;
}

/*
 * Function Name:    oem_dell_getsesmask
 *
 * Description:      This function calculates bits in SES drive update
 * Return:           Mask set with bits for SES backplane update
 */
static
int oem_dell_getsesmask(int argc, char **argv)
{
	int mask = 0;
	while(current_arg < argc) {
		if(!strcmp(argv[current_arg], "present"))
			mask |= (1L << 0);
		if(!strcmp(argv[current_arg], "online"))
			mask |= (1L << 1);
		if(!strcmp(argv[current_arg], "hotspare"))
			mask |= (1L << 2);
		if(!strcmp(argv[current_arg], "identify"))
			mask |= (1L << 3);
		if(!strcmp(argv[current_arg], "rebuilding"))
			mask |= (1L << 4);
		if(!strcmp(argv[current_arg], "fault"))
			mask |= (1L << 5);
		if(!strcmp(argv[current_arg], "predict"))
			mask |= (1L << 6);
		if(!strcmp(argv[current_arg], "critical"))
			mask |= (1L << 9);
		if(!strcmp(argv[current_arg], "failed"))
			mask |= (1L << 10);
		current_arg++;
	}
	return mask;
}

/*
 * Function Name:       oem_dell_setled_main
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
static
int oem_dell_setled_main(struct ipmi_intf *intf, int argc, char **argv)
{
	int bus = 0, device = 0, function = 0, mask = 0;
	int bayid, slotid;
	bayid = 0xff;
	slotid = 0xff;
	current_arg++;
	if(argc < current_arg) {
		usage();
		return -1;
	}
	/* ipmitool delloem setled info*/
	if(argc == 1 || strcmp(argv[current_arg], "help") == 0) {
		oem_dell_setled_usage();
		return 0;
	}
	oem_dell_checksetledsupport(intf);
	if(!oem_dell_issetledsupported()) {
		lprintf(LOG_ERR, 
				"'setled' is not supported on this system.");
		return -1;
	} else if(sscanf(argv[current_arg], "%*x:%x:%x.%x", &bus, &device, &function) == 3) {
		/* We have bus/dev/function of drive */
		current_arg++;
		oem_dell_getdrivemap(intf, bus, device, function, &bayid, &slotid);
	} else if(sscanf(argv[current_arg], "%x:%x.%x", &bus, &device, &function) == 3) {
		/* We have bus/dev/function of drive */
		current_arg++;
	} else{
		oem_dell_setled_usage();
		return -1;
	}
	/* Get mask of SES flags */
	mask = oem_dell_getsesmask(argc, argv);
	/* Get drive mapping */
	if(oem_dell_getdrivemap(intf, bus, device, function, &bayid, &slotid)) {
		return -1;
	}
	/* Set drive LEDs */
	return oem_dell_setled_state(intf, bayid, slotid, mask);
}

